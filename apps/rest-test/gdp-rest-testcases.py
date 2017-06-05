#!/usr/bin/env python
#
#	RESTful interface to GDP - Test Suite
#
#	----- BEGIN LICENSE BLOCK -----
#	Applications for the Global Data Plane
#	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
#
#	Copyright (c) 2017, Regents of the University of California.
#	All rights reserved.
#
#	Permission is hereby granted, without written agreement and without
#	license or royalty fees, to use, copy, modify, and distribute this
#	software and its documentation for any purpose, provided that the above
#	copyright notice and the following two paragraphs appear in all copies
#	of this software.
#
#	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
#	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
#	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
#	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
#	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
#	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
#	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
#	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
#	OR MODIFICATIONS.
#	----- END LICENSE BLOCK -----

import os
import sys
import requests
import json
import fcntl
import re
import socket
import subprocess

#
# Monitor the gdp-rest-v2.log for diagnostic detail
#
log_path = "/var/log/gdp/gdp-rest-v2.log"
log = open(log_path)
log.seek(0, os.SEEK_END)
log_fd = log.fileno()
flags = fcntl.fcntl(log_fd, fcntl.F_GETFL)
fcntl.fcntl(log_fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

# dev null to silence subprocess output
dn = open("/dev/null", "w")

#
# SHARED
#

test_auth = None
#
# Create a temporary test account on gdp-rest-01 (do not use sample strings):
#
# $cat /etc/lighttpd/.plainauth
# your_unique_testuser:your_unique_testpassword
# [other_permanent_user_accounts_left_untouched]
# $
#
# ...fill in test_auth to match the above, and uncomment it
#
# test_auth = ("your_unique_testuser", "your_unique_testpassword")

if test_auth == None:
    print "Error: modify gdp-rest-testcases.py to use a temporary test account"
    sys.exit(1)

json_header = { 'Content-type': 'application/json' }

def search_json_value(regexp, content):
    value = None
    line_m = re.search(regexp, content)
    if line_m != None:
        json_value_m = re.search(".*\:.*(\".*\")", line_m.group(1))
        if json_value_m != None:
            value = json_value_m.group(1)
    return value
#

# clean up test GCLs (defined to be gdp-rest-01 local GCLs), while
# being careful to only remove keys which go with test GCLs, as all
# other keys are for real GCLs located in the real GDP.
def clean_gcls_and_keys():
    output = subprocess.check_output([ "sudo", "-g", "gdp", "-u", "gdp",
                                       "/usr/bin/find",
                                       "/var/swarm/gdp/gcls/", 
                                       "-type", "f", "-name", "*.gdpndx" ])

    lines = output.splitlines()
    for line in lines:
        # find <gcl_id> in /var/swarm/gdp/gcls/<gcl_id>.gdpndx
        gcl_id = line[24:-7]
        # check expectations, where file removal is involved
        if len(gcl_id) == 43:
            print "Info: remove local GCL {}...".format(gcl_id),
            subprocess.call([ "sudo", "-g", "gdp", "-u", "gdp",
                              "/usr/bin/find", "/var/swarm/gdp/gcls/", 
                              "-type", "f", "-name", gcl_id + "*",
                              "-exec", "/bin/rm", "-f", "{}", ";"])
            print "done"
            print "Info: remove local key {}...".format(gcl_id),
            subprocess.call([ "sudo", "-g", "gdp", "-u", "gdp",
                              "/usr/bin/find", "/etc/gdp/keys/", 
                              "-type", "f", "-name", gcl_id + ".pem",
                              "-exec", "/bin/rm", "-f", "{}", ";"])
            print "done"
        else:
            print "Error: cleanup has unexpected gcl_id: {}".format(gcl_id)

#
# HTTP/HTML responses
#
def page_display(resp_page):
    if resp_page != None:
        print "STATUS:"
        print resp_page.status_code
        print "HEADERS:"
        print resp_page.headers
        print "CONTENT:"
        print resp_page.content
    else:
        print "Error: no response page"
#

#
# HTTP PUT
#

def test_put_01():
    log.seek(0, os.SEEK_END)
    test_case = "TEST PUT 01:"
    print "{} HTTP PUT new log".format(test_case)


    json_body = {
        "external-name" : "edu.berkeley.eecs.gdp-rest.test_put_01",
        "-s" : "edu.berkeley.eecs.gdp-rest-01.gdplogd",
    }

    page = requests.put("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 201:
        print "[FAILED]",
        passed = False
    print ""

    gcl_name = search_json_value(".*(\"gcl_name.*\").*", page.content)
    print "{} gcl_name: {}".format(test_case, gcl_name),
    if gcl_name == None:
        print "[FAILED]",
        passed = False
    print ""

    gdplogd_name = search_json_value(".*(\"gdplogd_name.*\").*", page.content)
    print "{} gdplogd_name: {}".format(test_case, gdplogd_name),
    if gdplogd_name == None:
        print "[FAILED]",
        passed = False
    print ""

    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_put_02():
    log.seek(0, os.SEEK_END)
    test_case = "TEST PUT 02:"
    print "{} HTTP PUT existing (from TEST PUT 01) log".format(test_case)
    
    json_body = {
        "external-name" : "edu.berkeley.eecs.gdp-rest.test_put_01",
        "-s" : "edu.berkeley.eecs.gdp-rest-01.gdplogd",
    }

    page = requests.put("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 409:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"external-name already exists on gdplogd server\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_put_03():
    log.seek(0, os.SEEK_END)
    test_case = "TEST PUT 03:"
    print "{} HTTP PUT with no external-name".format(test_case)
    
    json_body = {
        "NO-external-name" : "edu.berkeley.eecs.gdp-rest.test_put_03"
    }

    page = requests.put("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"mandatory external-name not found\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_put_04():
    log.seek(0, os.SEEK_END)
    test_case = "TEST PUT 04:"
    print "{} HTTP PUT with no request body".format(test_case)
    
    page = requests.put("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header)

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"request body not recognized json format\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_put_05():
    log.seek(0, os.SEEK_END)
    test_case = "TEST PUT 05:"
    print "{} HTTP PUT with no request header or body".format(test_case)
    
    page = requests.put("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60)

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"request body not recognized json format\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_put_06():
    log.seek(0, os.SEEK_END)
    test_case = "TEST PUT 06:"
    print "{} HTTP PUT new log, verify all valid options".format(test_case)


    json_body = {
        "external-name" : "edu.berkeley.eecs.gdp-rest.test_put_06",
        "-C" : "swarmlab@berkeley.edu",
        "-h" : "sha224",
        "-k" : "dsa",
        "-b" : "1024",
        "-c" : "ignored_for_dsa",
        "-s" : "edu.berkeley.eecs.gdp-rest-01.gdplogd",
        "META" : [ "meta1=foo", "meta2=bar" ],
    }

    page = requests.put("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 201:
        print "[FAILED]",
        passed = False
    print ""

    gcl_name = search_json_value(".*(\"gcl_name.*\").*", page.content)
    print "{} gcl_name: {}".format(test_case, gcl_name),
    if gcl_name == None:
        print "[FAILED]",
        passed = False
    print ""

    gdplogd_name = search_json_value(".*(\"gdplogd_name.*\").*", page.content)
    print "{} gdplogd_name: {}".format(test_case, gdplogd_name),
    if gdplogd_name == None:
        print "[FAILED]",
        passed = False
    print ""

    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_put_07():
    log.seek(0, os.SEEK_END)
    test_case = "TEST PUT 07:"
    print "{} HTTP PUT create log, -e option is not permitted".format(test_case)
    
    json_body = {
        "external-name" : "edu.berkeley.eecs.gdp-rest.test_put_07",
        "-e" : "none"
    }

    page = requests.put("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"request contains unrecognized json objects\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_put_08():
    log.seek(0, os.SEEK_END)
    test_case = "TEST PUT 08:"
    print "{} HTTP PUT create log, -K option is not permitted".format(test_case)
    
    json_body = {
        "external-name" : "edu.berkeley.eecs.gdp-rest.test_put_08",
        "-K" : "/etc/gdp/keys"
    }

    page = requests.put("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"request contains unrecognized json objects\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_put_09():
    log.seek(0, os.SEEK_END)
    test_case = "TEST PUT 09:"
    print "{} HTTP PUT create log, -q option is not permitted".format(test_case)
    
    json_body = {
        "external-name" : "edu.berkeley.eecs.gdp-rest.test_put_08",
        "-q" : None
    }

    page = requests.put("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"request contains unrecognized json objects\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_put_10():
    log.seek(0, os.SEEK_END)
    test_case = "TEST PUT 10:"
    print "{} HTTP PUT create log, META list is None(s)".format(test_case)
    
    json_body = {
        "external-name" : "edu.berkeley.eecs.gdp-rest.test_put_08",
        "META" : None
    }

    page = requests.put("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"request contains unrecognized json objects\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_put_11():
    log.seek(0, os.SEEK_END)
    test_case = "TEST PUT 11:"
    print "{} HTTP PUT create log, META list element bad(s)".format(test_case)
    
    json_body = {
        "external-name" : "edu.berkeley.eecs.gdp-rest.test_put_08",
        "META" : [ "foo=1", "missing_equal", "bar=2" ],
    }

    page = requests.put("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"request contains unrecognized json objects\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_put_12():
    log.seek(0, os.SEEK_END)
    test_case = "TEST PUT 12:"
    print "{} HTTP PUT create log, non-existant gdplogd name".format(test_case)
    
    json_body = {
        "external-name" : "edu.berkeley.eecs.gdp-rest.test_put_08",
        "-s" : "edu.berkeley.eecs.gdp-rest-01.DOESNOTEXIST.gdplogd"
    }

    page = requests.put("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"log server host not found\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

#
# HTTP POST
#

def test_post_01():
    log.seek(0, os.SEEK_END)
    test_case = "TEST POST 01:"
    print "{} HTTP POST new log".format(test_case)


    json_body = {
        "external-name" : None,
        "-s" : "edu.berkeley.eecs.gdp-rest-01.gdplogd",
    }

    page = requests.post("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 201:
        print "[FAILED]",
        passed = False
    print ""

    gcl_name = search_json_value(".*(\"gcl_name.*\").*", page.content)
    print "{} gcl_name: {}".format(test_case, gcl_name),
    if gcl_name == None:
        print "[FAILED]",
        passed = False
    print ""

    gdplogd_name = search_json_value(".*(\"gdplogd_name.*\").*", page.content)
    print "{} gdplogd_name: {}".format(test_case, gdplogd_name),
    if gdplogd_name == None:
        print "[FAILED]",
        passed = False
    print ""

    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_post_02():
    log.seek(0, os.SEEK_END)
    test_case = "TEST POST 02:"
    print "{} HTTP POST new log".format(test_case)


    json_body = {
        "external-name" : None,
        "-s" : "edu.berkeley.eecs.gdp-rest-01.gdplogd",
    }

    page = requests.post("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 201:
        print "[FAILED]",
        passed = False
    print ""

    gcl_name = search_json_value(".*(\"gcl_name.*\").*", page.content)
    print "{} gcl_name: {}".format(test_case, gcl_name),
    if gcl_name == None:
        print "[FAILED]",
        passed = False
    print ""

    gdplogd_name = search_json_value(".*(\"gdplogd_name.*\").*", page.content)
    print "{} gdplogd_name: {}".format(test_case, gdplogd_name),
    if gdplogd_name == None:
        print "[FAILED]",
        passed = False
    print ""

    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_post_03():
    log.seek(0, os.SEEK_END)
    test_case = "TEST POST 03:"
    print "{} HTTP POST with an external-name".format(test_case)
    
    json_body = {
        "external-name" : "edu.berkeley.eecs.gdp-rest.test_post_03"
    }

    page = requests.post("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"POST external-name must have null value\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_post_04():
    log.seek(0, os.SEEK_END)
    test_case = "TEST POST 04:"
    print "{} HTTP POST with no request body".format(test_case)
    
    page = requests.post("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header)

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"request body not recognized json format\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_post_05():
    log.seek(0, os.SEEK_END)
    test_case = "TEST POST 05:"
    print "{} HTTP POST with no request header or body".format(test_case)
    
    page = requests.post("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60)

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"request body not recognized json format\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_post_06():
    log.seek(0, os.SEEK_END)
    test_case = "TEST POST 06:"
    print "{} HTTP POST new log, verify all valid options".format(test_case)


    json_body = {
        "external-name" : None,
        "-C" : "swarmlab@berkeley.edu",
        "-h" : "sha224",
        "-k" : "dsa",
        "-b" : "1024",
        "-c" : "ignored_for_dsa",
        "-s" : "edu.berkeley.eecs.gdp-rest-01.gdplogd",
        "META" : [ "meta1=foo", "meta2=bar" ],
    }

    page = requests.post("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 201:
        print "[FAILED]",
        passed = False
    print ""

    gcl_name = search_json_value(".*(\"gcl_name.*\").*", page.content)
    print "{} gcl_name: {}".format(test_case, gcl_name),
    if gcl_name == None:
        print "[FAILED]",
        passed = False
    print ""

    gdplogd_name = search_json_value(".*(\"gdplogd_name.*\").*", page.content)
    print "{} gdplogd_name: {}".format(test_case, gdplogd_name),
    if gdplogd_name == None:
        print "[FAILED]",
        passed = False
    print ""

    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_post_07():
    log.seek(0, os.SEEK_END)
    test_case = "TEST POST 07:"
    print "{} HTTP POST create log, -e option is not permitted".format(test_case)
    
    json_body = {
        "external-name" : None,
        "-e" : "none"
    }

    page = requests.post("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"request contains unrecognized json objects\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_post_08():
    log.seek(0, os.SEEK_END)
    test_case = "TEST POST 08:"
    print "{} HTTP POST create log, -K option is not permitted".format(test_case)
    
    json_body = {
        "external-name" : None,
        "-K" : "/etc/gdp/keys"
    }

    page = requests.post("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"request contains unrecognized json objects\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_post_09():
    log.seek(0, os.SEEK_END)
    test_case = "TEST POST 09:"
    print "{} HTTP POST create log, -q option is not permitted".format(test_case)
    
    json_body = {
        "external-name" : None,
        "-q" : None
    }

    page = requests.post("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"request contains unrecognized json objects\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_post_10():
    log.seek(0, os.SEEK_END)
    test_case = "TEST POST 10:"
    print "{} HTTP POST create log, META list is None(s)".format(test_case)
    
    json_body = {
        "external-name" : None,
        "META" : None
    }

    page = requests.post("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"request contains unrecognized json objects\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_post_11():
    log.seek(0, os.SEEK_END)
    test_case = "TEST POST 11:"
    print "{} HTTP POST create log, META list element bad(s)".format(test_case)
    
    json_body = {
        "external-name" : None,
        "META" : [ "foo=1", "missing_equal", "bar=2" ],
    }

    page = requests.post("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"request contains unrecognized json objects\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

def test_post_12():
    log.seek(0, os.SEEK_END)
    test_case = "TEST POST 12:"
    print "{} HTTP POST create log, non-existant gdplogd name".format(test_case)
    
    json_body = {
        "external-name" : None,
        "-s" : "edu.berkeley.eecs.gdp-rest-01.DOESNOTEXIST.gdplogd"
    }

    page = requests.post("https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl",
                        auth = test_auth,
                        timeout = 60,
                        headers = json_header,
                        data = json.dumps(json_body))

    passed = True
    print "{} status code: {}".format(test_case, page.status_code),
    if page.status_code != 400:
        print "[FAILED]",
        passed = False
    print ""

    detail = search_json_value(".*(\"detail.*\").*", page.content)
    print "{} detail: {}".format(test_case, detail),
    if detail != "\"log server host not found\"":
        print "[FAILED]",
        passed = False
    print ""
    
    print "{}".format(test_case),
    if passed:
        print "PASSED"
    else:
        print "FAILED"
        print "DIAGNOSTICS:"
        print "==== gdp-rest-v2.log:"
        print log.read(-1)
        print "==== end"
    
        print "==== gdp-rest-01 response:"
        page_display(page)
        print "==== end"
        print "END DIAGNOSTICS"
#

#
# TEST prep
#
testbed = socket.gethostname()
if testbed != "gdp-rest-01":
    print "host is {}".format(testbed)
    print "Error: this script is only safe to run on the RESTful server"
    sys.exit(1)

print "checking is-active gdplogd (sudo systemctl is-active gdplogd) ..."
if subprocess.call("sudo systemctl is-active gdplogd",
                   shell=True, stdout=dn, stderr=dn) != 3:
    print "Error: gdplogd is already active on gdp-rest-01 (not normal case)"
    sys.exit(1)
print "inactive (expected)"

print "starting gdplogd (sudo systemctl start gdplogd.service) ..."
if subprocess.call("sudo systemctl start gdplogd.service",
                   shell=True) != 0:
    print "Error: systemctl start gdplogd.service failed"
    sys.exit(1)
print "started"
    
#
# TEST run
#

# HTTP PUT test suite
test_put_01()
test_put_02()
test_put_03()
test_put_04()
test_put_05()
test_put_06()
test_put_07()
test_put_08()
test_put_09()
test_put_10()
test_put_11()
test_put_12()

# HTTP POST test suite
test_post_01()
test_post_02()
test_post_03()
test_post_04()
test_post_05()
test_post_06()
test_post_07()
test_post_08()
test_post_09()
test_post_10()
test_post_11()
test_post_12()

print "stopping gdplogd (sudo systemctl stop gdplogd.service) ..."
if subprocess.call("sudo systemctl stop gdplogd.service",
                   shell=True) != 0:
    print "Error: systemctl stop gdplogd.service failed"
print "stopped"

clean_gcls_and_keys()

dn.close()
log.close()

sys.exit(0)
#
