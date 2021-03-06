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
#include <slib/std/cgi.hpp>
#include <slib/std/file.hpp>
#include <slib/std/online.hpp>
#include <slib/std/string.hpp>
#include <slib/std/url.hpp>

using namespace slib;


idx sCGI::grabPost=0;

// _/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
// _/
// _/ Initialization
// _/
// _/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/

sCGI * sCGI::init(idx argc, const char * * argv, const char * * envp, FILE * readfrom, bool isCookie, const char * forcedMethod)
{
    //FILE * readfrom=fopen(argv[1],"r");
    //putenv("REQUEST_METHOD=POST");// // DeBUGGING UNDER WINDOWS
    //setenv("REQUEST_METHOD","POST",1); // DeBUGGING UNDER LINUX

    if( grabPost != 0 || (strstr(argv[0], "debug")) )
        readfrom = sHtml::grabInData(stdin, envp);    // this is to debug the list of variable value pairs in HTML

    mangleNameChar = 0;
    sectionsToHide = 0;
    keywordTmpltHtml = "_tmplt.html";

    if( !readfrom ) {
        readfrom = stdin;
    }
    pForm = &form;
    sHtml::inputCGI(readfrom, argc, argv, pForm, mangleNameChar, isCookie, forcedMethod);

    // read some predefined parameters
    cmd = pForm->value("cmd");
    raw = pForm->ivalue("raw", 0);
    if( !cmd ) {
        cmd = pForm->value("cmdr"); // === raw=1
        if( cmd ) {
            sStr cmd_buf;
            cmd_buf.addString(cmd);
            pForm->inp("cmd", cmd_buf);
            if( !raw ) {
                pForm->inp("raw", "1");
                raw= 1;
            }
            cmd = pForm->value("cmd");
        }
    }

    sStr Htmldir;
    const char * customDir = pForm->value("sCgi_HtmlDir");
    if( customDir && customDir[0] ) {
        Htmldir.printf("%s,", customDir);
    }
    Htmldir.printf("./,lib/,tmpl/,util/");
    htmlDirs(Htmldir);

    const char * redir = pForm->value("follow");
    if( redir ) {
        redirectURL.printf("%s", redir);
    }
    bool isDebug = pForm->ivalue("debug") ? true : false;
    if( isDebug ) {
        sHtml::outFormData(pForm);
    }
    flOut = stdout;
    return this;
}

// _/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
// _/
// _/ Composers
// _/
// _/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/

void sCGI::outHeaders(sStr* str)
{
    if( !m_headersPrinted ) {
        if( !str ) {
            str = this;
        }
        const char* hcookie = "Set-Cookie";
        const char* hct = "Content-type";
        const char* huacomp = "X-UA-Compatible";
        bool has_ct = false;
        bool has_uacomp = false;
        for(idx i = 0; i < m_headers.dim(); i++) {
            const char* nm = (const char*)m_headers.id(i);
            has_ct |= strcasecmp(nm, hct) == 0;
            has_uacomp |= strcasecmp(nm, huacomp) == 0;
            if( strcasecmp(nm, hcookie) != 0 ) {
                // do not print cookies inserted as plain headers
                str->printf("%s: %s\r\n", nm, m_headers.value(nm));
            }
        }
        if( !has_ct ) {
            if( raw ) {
                str->printf("%s: text/plain\r\n", hct);
            } else {
                str->printf("%s: text/html\r\n", hct);
            }
        }
        if( !has_uacomp ) {
            // Avoid IE's compatibility mode if possible
            str->printf("%s: IE=Edge\r\n", huacomp);
        }
        for(idx i = 0; i < m_cookies.dim(); i++) {
            const char* nm = (const char*)m_cookies.id(i);
            str->printf("%s: %s=", hcookie, nm);
            URLEncode(m_cookies.value(nm), *str, eUrlEncode_PercentOnly);
            // path set to '/' to correlate with javascript, otherwise pages break
            str->printf("; Path=/\r\n");
        }
        str->printf("\r\n");
        m_headersPrinted = true;
    }
}

void sCGI::outHtml(sHtml * html)
{
    if( !html ) {
        html = this;
    }
    if( raw < 2 ) {
        sStr headers;
        outHeaders(&headers);
        if( headers ) {
            html->printf("%s", headers.ptr());
        }
    }
    if( raw ) {
        if( dataForm.length() )
            html->printf("%s", dataForm.ptr());
        return;
    }
    if( !(sectionsToHide & eSectionHideHeader) ) {
        outSection(html, "header");
    }
    if( redirectURL ) {
        //html->printf("<meta http-equiv='refresh' content='0;url='");
        //if( strncmp(redirectURL,"http:",5)!=0 ) html->printf("?cmd=");
        //html->printf("%s'>\n",redirectURL.ptr());
        if( strncmp(redirectURL, "http:", 5) != 0 && strncmp(redirectURL, "https:", 6) != 0 ) {
            execJS.printf("document.location='?cmd=%s';\n", redirectURL.ptr());
        } else {
            execJS.printf("document.location='%s';\n", redirectURL.ptr());
        }
    }
    if( execJS.length() ) {
        html->printf("<script>%s</script>", execJS.ptr());
    }
    if( !(sectionsToHide & eSectionHideTop) ) {
        outSection(html, "top");
    }
    if( htmlBody.length() ) {
        html->printf("%s", htmlBody.ptr());
    }
    if( dataForm.length() ) {
        html->printf("<form name='sCgi_DataForm'>%s</form>", dataForm.ptr());
    }
    if( !(sectionsToHide & eSectionHideBottom) ) {
        outSection(html, "bottom");
    }

    if( !(sectionsToHide & eSectionHideTail) ) {
        outSection(html, "tail");
    }
}

// _/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
// _/
// _/ Handlers
// _/
// _/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
void sCGI::headerSet(const char * nm, const char * fmt, ...)
{
    sStr str;
    sCallVarg(str.vprintf, fmt);
    m_headers.inp(nm, str.ptr());
}

void sCGI::headerSetContentDispositionAttachment(const char * flnm_fmt, ...)
{
    sStr str;
    sCallVarg(str.vprintf, flnm_fmt);
    if( !str.length() ) {
        m_headers.inp("Content-Disposition", "attachment;");
        return;
    }

    str.add0();
    idx offset = str.length();
    str.add(0, 32 + 4 * offset); // to avoid reallocs when appending to str
    const char * flnm = str.ptr();
    str.cut(offset);
    str.addString("attachment; filename");

    enum {
        eToken,
        eQuotedString,
        eUnicode
    } mode = eToken;
    for(idx i=0; i<offset-1; i++) {
        if( flnm[i] < ' ' || ((const unsigned char *)flnm)[i] > 127 ) {
            mode = eUnicode;
            break;
        } else if( !(flnm[i] >= '0' && flnm[i] <= '9') && !(flnm[i] >= 'a' && flnm[i] <= 'z') && !(flnm[i] >= 'A' && flnm[i] <= 'Z') && flnm[i] != '.' && flnm[i] != '-' && flnm[i] != '_' ) {
            mode = eQuotedString;
        }
    }

    const char * user_agent = pForm ? pForm->value("USER_AGENT") : 0;

    switch( mode ) {
        case eToken:
            str.addString("=");
            str.addString(flnm);
            break;
        case eQuotedString:
            str.addString("=\"", 2);
            str.addString(flnm);
            str.addString("\"", 1);
            break;
        case eUnicode:
            // browser-specific madness
            // see http://stackoverflow.com/questions/93551/how-to-encode-the-filename-parameter-of-content-disposition-header-in-http
            // http://blogs.msdn.com/b/ieinternals/archive/2010/06/07/content-disposition-attachment-and-international-unicode-characters.aspx
            if( user_agent && (strstr(user_agent, "MSIE 6.") || strstr(user_agent, "MSIE 7.0") || strstr(user_agent, "MSIE 8.0") || strstr(user_agent, "Android")) ) {
                str.addString("=\"", 2);
                URLEncode(flnm, str, eUrlEncode_ExtValue);
                str.addString("\"", 1);
            } else {
                str.addString("*=UTF-8''");
                URLEncode(flnm, str, eUrlEncode_ExtValue);
            }
            break;
    }

    m_headers.inp("Content-Disposition", str.ptr(offset));
}

void sCGI::headerDelete(const char * nm)
{
    m_headers.inp(nm, sStr::zero, 0);
}

void sCGI::cookieSet(const char * nm,const char * fmt, ... )
{
    sStr str;
    sCallVarg(str.vprintf, fmt);
    m_cookies.inp(nm, str.ptr());
}

void sCGI::cookieDelete(const char * nm )
{
    m_cookies.inp(nm, sStr::zero, 0);
}

void sCGI::alert(const char * fmt, ... )
{
    sStr str; sCallVarg(str.vprintf,fmt); if(!str.length())return ;
    if(raw)dataForm.printf("info:%s\n",str.ptr());
    else execJS.printf("alert('%s');\n", str.ptr() );
}

void sCGI::warning(const char * fmt, ... )
{
    sStr str; sCallVarg(str.vprintf,fmt); if(!str.length())return ;
    if(raw)dataForm.printf("warning:%s\n",str.ptr());
    else execJS.printf("alert('Warning: %s');\n", str.ptr() );
}

void sCGI::error(const char * fmt, ... )
{
    sStr str; sCallVarg(str.vprintf,fmt); if(!str.length())return ;
    if(raw)dataForm.printf("error:%s\n",str.ptr());
    else execJS.printf("alert('Error: %s');\n", str.ptr() );
}

void sCGI::linkSelf(const char * cmd,const char * fmt, ... )
{
    if(fmt){
        sStr str; sCallVarg(str.vprintf,fmt); if(!str.length()) return ;
        execJS.printf("linkSelf('%s&%s');\n", cmd, str.ptr() );
    }
    else execJS.printf("linkSelf('%s');\n", cmd );
}

void sCGI::executeJS(const char * fmt, ... )
{
    sCallVarg(execJS.vprintf,fmt);
}

const char* sCGI::selfURL(sStr& url)
{
    url.cut(0);
    const char* https = pForm->value("HTTPS", "no");
    bool is_https = (strcasecmp(https, "on") == 0 || strcasecmp(https, "yes") == 0);
    if( is_https ) {
        url.printf("https");
    } else {
        url.printf("http");
    }
    url.printf("://%s", pForm->value("HOST"));
    const char* port = pForm->value("PORT");
    if( port && port[0] && !(!is_https && strcmp("80", port) == 0) && !(is_https && strcmp("443", port) == 0) ) {
        url.printf(":%s", port);
    }
    url.printf("%s", pForm->value("SCRIPT_NAME"));
    return url.ptr();
}

void sCGI::exportData(const char * dataname, const char * fmt, ... )
{
    if(raw) {
        sCallVarg(dataForm.vprintf,fmt);
    } else {
        sStr str; sCallVarg(str.vprintf,fmt); if(!str.length())return ;
        dataForm.printf("<textarea name='%s' rows=2 cols=80 >%s</textarea>",dataname,str.ptr());
    }
}

idx sCGI::outSectionVa(sHtml * html, const char * sectionFmt, va_list marker)
{
    const char * h00 = (htmlDirs00.length() == 0) ? ""__ : htmlDirs00.ptr();
    if( !h00 ) {
        h00 = ""__;
    }
    sStr fl;
    for(const char * htmldir = h00; htmldir; htmldir = sString::next00(htmldir)) {
        sStr str("%s", htmldir);
        str.vprintf(sectionFmt, marker);
        str.printf("%s", keywordTmpltHtml);

        sStr buf;
        if( !sDir::aliasResolve(buf, "qapp.cfg", "[CGI]", str, true, false) ) {
            continue;
        }
        str.printf(0, "%s", buf.ptr());
        fl.init(str, sFil::fReadonly);
        if( fl.ok() ) {
            break;
        }
        fl.destroy();
    }
    if( fl ) {
        if( html ) {
            html->printf("%s", fl.ptr());
        } else {
            htmlBody.printf("%s", fl.ptr());
        }
    }
    return fl.length();
}

bool sCGI::checkETag(sStr & etagBuf, idx len, idx timeStamp)
{
    idx start = etagBuf.length();
    etagBuf.printf("%"DEC":%"DEC, timeStamp, len);
    const char * etagForm = pForm->value("IF_NONE_MATCH");
    return etagForm && strcmp(etagForm, etagBuf.ptr(start)) == 0;
}

void sCGI::outBin(const void * buf, idx len, idx timeStamp, bool asAttachment, const char * flnmFormat, ...)
{
    sStr etag;
    if( flnmFormat && timeStamp > 0 && checkETag(etag, len, timeStamp) ) {
        outBinCached(etag);
    } else {
        va_list ap;
        va_start(ap, flnmFormat);
        voutBinUncached(buf, len, etag, asAttachment, flnmFormat, ap);
        va_end(ap);
    }
}

void sCGI::outBinHeaders(bool asAttachment, const char * flnmFormat, ...)
{
    va_list ap;
    va_start(ap, flnmFormat);
    voutBinUncached(0, 0, 0, asAttachment, flnmFormat, ap);
    va_end(ap);
}

bool sCGI::outFile(const char * flnmReal, bool asAttachment, const char * flnmFormat, ...)
{
    if( !sFile::exists(flnmReal) )
        return false;

    idx len = sFile::size(flnmReal);
    idx timeStamp = sFile::time(flnmReal);
    sStr etag;
    if( checkETag(etag, len, timeStamp) ) {
        outBinCached(etag);
    } else {
        sFil f(flnmReal, sMex::fReadonly);
        if( !f.ok() )
            return false;

        va_list ap;
        va_start(ap, flnmFormat);
        voutBinUncached(f.ptr(), f.length(), etag, asAttachment, flnmFormat, ap);
        va_end(ap);
    }
    return true;
}

void sCGI::outBinCached(const char * etag)
{
    /* See https://developers.google.com/web/fundamentals/performance/optimizing-content-efficiency/http-caching
     * Ideally, we want a long max-age and the file's hash in the url. Until then, set a short max-age to force
     * the browser to re-check the ETag every 2 minutes. (If the file didn't change, we will give a 304 response,
     * so things will still be reasonably fast.) */
    headerSet("Status", "304");
    headerSet("ETag", "%s", etag);
    headerSet("Cache-Control", "private, max-age=120"); // 2 minutes
    sStr headers;
    outHeaders(&headers);
    fwrite(headers.ptr(), headers.length(), 1, flOut);
}

void sCGI::voutBinUncached(const void * buf, idx len, const char * etag, bool asAttachment, const char * flnmFormat, va_list marker)
{
    if( flnmFormat ) {
        sStr str;
        str.vprintf(flnmFormat, marker);
        headerSet("Content-Type", "%s", sHtml::contentTypeByExt(str.ptr()));
        if( asAttachment ) {
            sFilePath fn(str, "%%flnm");
            headerSetContentDispositionAttachment(fn.ptr());
        }
        if( len > 0 )
            headerSet("Content-Length", "%"DEC, len);
        if( etag ) {
            headerSet("ETag", "%s", etag);
            headerSet("Cache-Control", "private, max-age=120"); // 2 minutes - see comment in outBinCached()
        }
    }
    sStr headers;
    outHeaders(&headers);
    fwrite(headers.ptr(), headers.length(), 1, flOut);
    fwrite(buf, len, 1, flOut);
}

void sCGI::htmlDirs(const char * dirs)
{
    htmlDirs00.cut(0);
    sString::searchAndReplaceSymbols(&htmlDirs00, dirs, 0, ",", 0, 0, true, true, true);
}


idx sCGI::Cmd(const char *)
{
    if( !cmd ) {
        if( pForm->value("f") || pForm->value("F") ) {
            cmd = "file";
        } else {
            return 0;
        }
    }
    if( strcmp(cmd, "proxy") == 0 ) {
        const char * url = pForm->value("url", 0);
        if( !url ) {
            return 0;
        }
        sMex http;
        sConClient::getHTTP(&http, url);
        if( http.pos() ) {
            fwrite(http.ptr(), http.pos(), 1, flOut);
        }
        raw = 1;
        return 1;
    } else if( strcmp(cmd, "re-blob") == 0 ) {
        idx sz = 0;
        const char * blob = pForm->value("blob", 0, &sz);
        const char * filename = pForm->value("filename", 0);
        if( !blob || !sz ) {
            return 0;
        }
        outBin(blob, sz, 0, true, "%s", filename);
        return 1;
    } else if( strcmp(cmd, "file") == 0 ) {
        const char * filename = pForm->value("filename", 0);
        if( !filename ) {
            filename = pForm->value("f", 0);
        }
        if( !filename ) {
            filename = pForm->value("F", 0);
        }
        sStr buf;
        bool valid_name = false;
        // TODO : allow globs that match a whitelist
        if( sDir::aliasResolve(buf, "qapp.cfg", "[CGI]", filename, true, false) ) {
            valid_name = true;
            for( const char * resolved_name = buf; resolved_name; resolved_name = sString::next00(resolved_name) ) {
                // sanity check: all results must be relative, non-".." filenames
                if( resolved_name[0] == '/' || strstr(resolved_name, "../") ) {
                    valid_name = false;
                    break;
                }
            }

            if( valid_name ) {
                if( pForm->ivalue("nameonly", 0) ) {
                    for(const char * p = buf; p; p = sString::next00(p)) {
                        dataForm.printf("%s\n", p);
                    }
                    outHtml();
                } else if( !outFile(buf, pForm->boolvalue("attachment"), "%s", filename) ) {
                    buf.cut(0);
                }
            }
        }
        if( !valid_name ) {
            headerSet("Status", "404");
            outHtml();
        }

        return 1;
    }
    return 0;
}

idx sCGI::CmdStatic(const char * cmd)
{
    idx ret=outSection(cmd);
    if(ret)
        outHtml();
    return ret;
}

idx sCGI::run(const char * rcmd)
{
    if( sFile::size("capture.html") != 0 ) {
        sStr t;
        outHeaders(&t);
        ::printf("%s<html><head><meta http-equiv=\"refresh\" content=\"0;url=capture.html\"></head></html>", t.ptr());
        return 0;
    }
    if( !OnCGIInit() ) {
        return 0;
    }
    if( !rcmd ) {
        rcmd = cmd;
    }
    idx ret = Cmd(rcmd);
    cmd = pForm->value("cmd");
    if( !ret ) {
        ret = CmdStatic(rcmd);
    }
    if( !ret ) {
        headerSet("Status", "404");
        if( raw == 0 ) {
            outSection("unknown");
        }
        outHtml();
    }
    return ret;
}

