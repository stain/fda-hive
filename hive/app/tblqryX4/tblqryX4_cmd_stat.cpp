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

#include <slib/utils.hpp>
#include <ssci/math.hpp>
//#include <ssci/math/stat/stat.hpp>
#include "tblqryX4_cmd.hpp"

using namespace slib;
using namespace slib::tblqryx4;

#define PRFX "stat-"
#define OUTFILE "stat.csv"

/*

sIndex < sIndexStruc >  a;

class MYIJO {
        idx defint;
        idx ofsetMyData;
};
sVec< real > v;

 // how to put your data into sIndex
MYIJO ijo;
ijo.offsetMyData=v.dim();
a.add(ij,(sIndexStruc *)&ijo);

v.add(k+3);


// how to find your data by ij
ref=a.find(ij);
if( !ref  )
    err // didn't find ij
else
    ofs=a[ref-1].offsetMyData;


// scan through elements you have put into one by one
for( idx i=0; i<a.dim(); ++i) {
    ij=a[i].defint;
    ofs=a[i].offsetMyData
}
*/

namespace slib {
    namespace tblqryx4 {
        class StatTestCommand : public Command
        {
            private:
                sVec <idx> colSet1, colSet2;
                sVec <idx> rowSet;
                sVec <idx> uid;

            public:
                StatTestCommand(ExecContext & ctx) : Command(ctx)
                {
                }

                const char * getName() { return "statTest"; }
                bool computesOutTable() { return false; }
                bool needsInTableReinterpret() { return true; }

                bool init(const char * op_name, sVariant * arg);
                bool compute(sTabular * tbl);
        };
        Command * cmdStatTestFactory(ExecContext & ctx) { return new StatTestCommand(ctx); }
    };
};

bool StatTestCommand::init(const char * op_name, sVariant * arg)
{
    if (sVariant * colSetVal = arg->getDicElt("colSet1"))
    {
        const char * p=colSetVal->asString();if(*p=='[')++p;
        sString::scanRangeSet(p,0,&(colSet1),0ll,0ll,0ll);
    }
    if (sVariant * colSetVal = arg->getDicElt("colSet2"))
    {
        const char * p=colSetVal->asString();if(*p=='[')++p;
        sString::scanRangeSet(p,0,&(colSet2),0ll,0ll,0ll);
    }
    if (sVariant * rowSetVal = arg->getDicElt("rowSet"))
    {
        const char * p=rowSetVal->asString();if(*p=='[')++p;
        sString::scanRangeSet(p,0,&(rowSet),0ll,0ll,0ll);
    }
    if (sVariant * rowSetVal = arg->getDicElt("uid"))
    {
        const char * p=rowSetVal->asString();if(*p=='[')++p;
        sString::scanRangeSet(p,0,&(uid),0ll,0ll,0ll);
    }

    return true;
}

//student t-test here
bool StatTestCommand::compute(sTabular * tbl)
{
    sMatrix Mat;
    sDic <idx> rowIDs, colIDs;
    sVec <idx> allCols;

    char c [16];
    sprintf(c, "0-%"DEC, tbl->cols());
    sString::scanRangeSet(c,0, &allCols,0ll,0ll,0ll);


    if (!rowSet || rowSet.dim() < 1)
    {
        char p [16];
        sprintf (p, "0-%"DEC, tbl->rows());
        sString::scanRangeSet(p,0,&(rowSet),0ll,0ll,0ll);
    }

    Mat.parseTabular(tbl, &rowSet, &allCols, &colIDs, &rowIDs, 0, 0, true);

    sStr pathT;
    _ctx.qproc().reqSetData(_ctx.outReqID(),"file://"OUTFILE,0,0);
    _ctx.qproc().reqDataPath(_ctx.outReqID(),OUTFILE,&pathT);
    sFile::remove(pathT);
    sFil out(pathT);

    pathT.cut(0);
    _ctx.qproc().reqSetData(_ctx.outReqID(),"file://graphData.csv",0,0);
    _ctx.qproc().reqDataPath(_ctx.outReqID(),"graphData.csv",&pathT);
    sFile::remove(pathT);
    sFil out2(pathT);

    real * probvals =(real*)malloc(rowSet.dim()*sizeof(real));
    real * tstatvals=(real *)malloc(rowSet.dim()*sizeof(real));

    out.printf ("Row,P-value,T-value\n");
    out2.printf ("Row,Log P\n");

    sVec < sVec < idx > > pairset;pairset.add(2);
    for ( idx ii=0; ii < colSet1.dim(); ++ii)
        pairset[0].vadd(1,colSet1[ii]);
    for ( idx ii=0; ii < colSet2.dim(); ++ii)
        pairset[1].vadd(1,colSet2[ii]);

    sStat::statTest(Mat.ptr(0,0), Mat.rows(), Mat.cols(), &pairset, probvals, tstatvals);

    if (!uid || uid.dim() == 0)
        uid.vadd(1,0);

    for (idx i=0; i < rowSet.dim(); i++)
    {
        sStr outStr;
        for( idx iu=0; iu<uid.dim(); ++iu  ) {
            if(iu)
                out.printf(" ");
            tbl->printCell(outStr, rowSet[i],uid[iu]);
        }
        out.printf("%s,%lf,%lf\n", outStr.ptr(), probvals[i], tstatvals[i]);

        if (atof(outStr.ptr()))
            out2.printf("%f,%lf\n", atof(outStr.ptr()), -(log10(probvals[i])/log10(2)));
        else
            out2.printf("%"DEC",%lf\n", i, -(log10(probvals[i])/log10(2)));
    }

    free (probvals);
    free (tstatvals);

    return true;
}

