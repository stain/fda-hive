/*
 *  ::718604!
 * 
 * Copyright(C) November 20, 2014 U.S. Food and Drug Administration
 * Authors: Dr. Vahan Simonyan (1), Dr. Raja Mazumder (2), et al
 * Affiliation: Food and Drug Administration (1), George Washington University (2)
 * 
 * All rights Reserved.
 * 
 * The MIT License (MIT)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "dna-profx.hpp"

idx DnaProfXvarscan::PrepareData ( sUsr& user, const char * parentIDs, const char * workDir, sStr &errMsg)
{

    //
    // Check to make sure that the computation is based on an alignment
    //
    if (!parentIDs) {
        // Error: the aligner ID is missing from the pForm
        qp->reqSetInfo(qp->reqId, qp->eQPInfoLevel_Error, "No Alignment selected.");
        qp->reqSetStatus(qp->reqId, qp->eQPReqStatus_SysError);
        return 0;
    }

    sHiveId parid(parentIDs);
    sUsrObj profile(user, parid);
    if ( !profile.Id() ) {
        // Error: the profile ID is missing
        qp->reqSetInfo(qp->reqId, qp->eQPInfoLevel_Error, "No profile ID specified.");
        qp->reqSetStatus(qp->reqId, qp->eQPReqStatus_SysError);
        return 0;
    }

    //
    // Load the alignment data
    // If missing then SAMtools cannot be run
    //
    sStr path;
    profile.getFilePathname00(path, "alignment.hiveal"_"alignment.vioal"__);
    if (!path || (sFile::size(path.ptr())==0) ) {
        // Error: the alignment file appears to be empty
        qp->reqSetInfo(qp->reqId, qp->eQPInfoLevel_Error, "Alignments are missing or corrupted.");
        qp->reqSetStatus(qp->reqId, qp->eQPReqStatus_SysError);
        return 0;
    }
    sHiveal hiveal(&user, path);
    sBioal * bioal = &hiveal;
    if (!bioal || (bioal->dimAl()==0) ) {
        // Error: the alignment file is not empty but has no alignments in it.
        qp->reqSetInfo(qp->reqId, qp->eQPInfoLevel_Error, "Alignment object contains no alignments.");
        qp->reqSetStatus(qp->reqId, qp->eQPReqStatus_SysError);
        return 0;
    }

    //
    // Load the subject
    // If not available SAMTools cannot be run
    //
    const char * subject=profile.propGet00("subject", 0, ";");
    sHiveseq Sub(&user, subject, hiveal.getSubMode());
    if(Sub.dim()==0) {
        // Error: the subject appears to be empty
        qp->reqSetInfo(qp->reqId, qp->eQPInfoLevel_Error, "References are missing or corrupted.");
        qp->reqSetStatus(qp->reqId, qp->eQPReqStatus_SysError);
        return 0;
    }

    //
    // Load the query/reads file(s).
    // If not available then SAMTools cannot be run.
    //
    const char * query=profile.propGet00("query", 0, ";");
    sHiveseq Qry(&user, query, hiveal.getQryMode());
    if(Qry.dim()==0) {
        // Error: there appears to be no query/reads
        qp->reqSetInfo(qp->reqId, qp->eQPInfoLevel_Error, "Queries are missing or corrupted.");
        qp->reqSetStatus(qp->reqId, qp->eQPReqStatus_SysError);
        return 0;
    }



    //
    // Go through each alignment set by subject
    // This allows us to produce one VCF file after another using
    // SAMtools in order to decrease the resources required by each run.
    // Ultimately this could be parallelized.
    //
    sStr spath;
    sStr samAlignmentFile;
    for (idx ii = 0; ii < Sub.dim(); ii++) {
        spath.cut(0);
        samAlignmentFile.printf(0,"%s/alignment-%"DEC".sam", workDir, ii);
        profile.getFilePathname(spath, "alignment-%"DEC".sam", ii);

        //
        // Copy the alignment file into the work directory if the SAM file already exists
        //
        if (spath && sFile::size(spath.ptr())>0) {
            sFile::copy(spath.ptr(), samAlignmentFile.ptr(), false);
        } else {
            // SAM file doesn't exist in the object directory so
            // convert .vioalt into .sam
            sViosam::convertVioaltIntoSam(bioal, ii, &Qry, &Sub, true, samAlignmentFile);
        }

        //
        // Data should be written into the alignment-x.sam file
        // Verify that the file is not empty otherwise there may be a writing
        // error (at the minimum even with no alignments the header should be written into the file).
        if (!sFile::size(samAlignmentFile.ptr())) {
            qp->reqSetInfo(qp->reqId, qp->eQPInfoLevel_Error, "Could not access created SAM file.");
            qp->reqSetStatus(qp->reqId, qp->eQPReqStatus_SysError);
            qp->logOut(qp->eQPLogType_Error, "Could not access created SAM file.");
            return 0;
        }

        //
        // Dump reference into fasta file in working directory with full reference names
        // References names are, however, truncated at the first space (if applicable)
        // due to requirements in the SAM format specification.  SAMtools cannot properly
        // work with reference IDs that contain spaces.
        //
        sStr subjectFastaFile("%s/subject-%"DEC".fa", workDir, ii);
        sFil subjectFastaFil(subjectFastaFile.ptr());
        Sub.printFastXRow(&subjectFastaFil, false, ii, 0, 0, 0, true, false, 0, 0, 0, true, false, false, true);

        //
        // Check to see if the reference successfully was created
        //
        if (!subjectFastaFile || sFile::size(subjectFastaFile.ptr())==0) {
            // Error: the reference file appears to not exist, or is empty
            qp->reqSetInfo(qp->reqId, qp->eQPInfoLevel_Error, "Could not access dumped reference file.");
            qp->reqSetStatus(qp->reqId, qp->eQPReqStatus_SysError);
            qp->logOut(qp->eQPLogType_Error, "Could not access created dumped reference file.");
            return 0;
        }
    }
    return 1;
}


idx DnaProfXvarscan::Profile (sIO * log, sStr * outFile, const char * workDir, sUsr& user, const char * parentIDs, const char * additionalCommandLineParameters/*=0*/)
{
    // load the subject
    sHiveId parid(parentIDs);
    sUsrObj profile(user, parid);
    if ( !profile.Id() ) {
        qp->reqSetInfo(qp->reqId, qp->eQPInfoLevel_Error, "Profile missing or corrupted.");
        qp->reqSetStatus(qp->reqId, qp->eQPReqStatus_SysError);
        qp->logOut(qp->eQPLogType_Error, "Failed to access user profile.");
        return 0;
    }

    const char * subject=profile.propGet00("subject", 0,  ";");
    sHiveseq Sub(&user, subject);
    Sub.reindex();
    if(Sub.dim()==0) {
        qp->reqSetInfo(qp->reqId, qp->eQPInfoLevel_Error, "Subject missing or corrupted.");
        qp->reqSetStatus(qp->reqId, qp->eQPReqStatus_SysError);
        qp->logOut(qp->eQPLogType_Error, "Subject missing or corrupted.");
        return 0; // error
    }

    sStr vpath;
    bool skipHeader = false;
    for (idx i = 0; i < Sub.dim(); i++){
        sStr subjectFastaFile("%s/subject-%"DEC".fa",workDir, i);

        //sStr cmdLine("samtoolsVCF.os"SLIB_PLATFORM" \'%s\' \'%s\'", workDir, subjectFastaFile.ptr());
        // Remove the OS; this is set at the script level (?)
        sStr cmdLine("\"%sdna-profx.sh.os%s\" %s run --workDir \'%s\' --subjectFastaFile \'%s\' --subID %"DEC"", resourceRoot.ptr(), SLIB_PLATFORM, algorithm.ptr() ,workDir, subjectFastaFile.ptr(), i);

        if(log)log->printf("RUNNING: %s\n",cmdLine.ptr());
        qp->logOut(qp->eQPLogType_Error, "Launching SAMTools Variant caller.");

        sPS::execute(cmdLine);

        outFile->cut(0);
        outFile->printf("%s/SNP-%"DEC".vcf", workDir, i);

        // Copy output from working directory to object directory
        vpath.cut(0);
        qp->reqAddFile(vpath, "SNP-%"DEC".vcf", i);
        if (vpath && sFile::exists( outFile->ptr() ) &&( sFile::size( outFile->ptr() ) > 0 ) ) {
            sFile::copy( outFile->ptr() , vpath.ptr() , false);
        }
        else {
            qp->reqSetInfo(qp->reqId, qp->eQPLogType_Info, "%s", cmdLine.ptr());
            qp->reqSetInfo(qp->reqId, qp->eQPInfoLevel_Error, "request %"DEC" failed to produce SNP.vcf file", qp->reqId);
            qp->reqSetStatus(qp->reqId, qp->eQPReqStatus_ProgError);
            return 0;
        }

        //convert .vcf into .csv file
        sStr SNPprofileFileTmplt;
        qp->reqAddFile(SNPprofileFileTmplt, "SNPprofile-%"DEC, i);
        sViosam viosam;

        //
        // Create the CSV file HIVE uses from the VCF file generated by SAMTOOLS
        //
        idx ret = viosam.convertVarScan2OutputintoCSV(vpath.ptr(), SNPprofileFileTmplt.ptr(), &Sub, false);
        //idx ret = viosam.convertVCFintoCSV(vpath.ptr(), SNPprofileFileTmplt.ptr(), &Sub, skipHeader);
        if (ret==0) {
            qp->reqSetInfo(qp->reqId, qp->eQPInfoLevel_Error, "request %"DEC" could not convert VCF file into CSV format.", qp->reqId);
            qp->reqSetStatus(qp->reqId, qp->eQPReqStatus_SysError);
            return 0;
        }
        //
        // Set skip header flag to true for concatination in finalization
        //
        skipHeader=true;
        //
        // TODO: Create the Frequency Profile and Noise profile here
        //


    }
    return 1;
}

idx DnaProfXvarscan::Finalize (sIO * log, sStr * outFile, const char * workDir, sUsr& user, const char * parentIDs, const char * additionalCommandLineParameters/*=0*/)
{
    // load the subject
    sHiveId parid(parentIDs);
    sUsrObj profile(user, parid);
    if ( !profile.Id() ) {
        qp->reqSetInfo(qp->reqId, qp->eQPInfoLevel_Error, "Profile missing or corrupted.");
        qp->reqSetStatus(qp->reqId, qp->eQPReqStatus_SysError);
        qp->logOut(qp->eQPLogType_Error, "Failed to access user profile.");
        return 0;
    }

    const char * subject=profile.propGet00("subject", 0,  ";");
    sHiveseq Sub(&user, subject);Sub.reindex();
    if(Sub.dim()==0) {
        qp->reqSetInfo(qp->reqId, qp->eQPInfoLevel_Error, "Subject missing or corrupted.");
        qp->reqSetStatus(qp->reqId, qp->eQPReqStatus_SysError);
        qp->logOut(qp->eQPLogType_Error, "Subject missing or corrupted.");
        return 0; // error
    }

    // Create parent Profile File
    sStr SNPprofileFileTmplt;
    qp->reqAddFile(SNPprofileFileTmplt, "SNPprofile.csv");

    sStr ProfileChunk;
    for (idx i = 0 ;i < Sub.dim(); i++){
        ProfileChunk.cut(0);
        qp->reqGetFile(ProfileChunk,"SNPprofile-%"DEC".csv", i);
        sFile::copy( ProfileChunk.ptr(), SNPprofileFileTmplt.ptr() , true);

        // Remove profile chunk here since everything is now concatinated into SNPprofile.csv
        // Maintain VCFs for the time being
        sFile::remove(ProfileChunk);
    }

    return 1;
}

