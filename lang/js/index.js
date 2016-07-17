// Copyright (c) 2016 The Regents of the University of California.
// All rights reserved.

// Permission is hereby granted, without written agreement and without
// license or royalty fees, to use, copy, modify, and distribute this
// software and its documentation for any purpose, provided that the above
// copyright notice and the following two paragraphs appear in all copies
// of this software.

// IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY
// FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
// ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
// THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

// THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE
// PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE UNIVERSITY OF
// CALIFORNIA HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
// ENHANCEMENTS, OR MODIFICATIONS.

/**
 * Node module for the Global Data Plane.
 *
 * @module gdp
 * @author Christopher Brooks
 * @version $$Id: gdp.js 838 2016-06-23 22:08:30Z cxh $$
 */

var fs = require('fs');
var ref = require('ref');
var LIBGDP_H_DIR = './gdpjs/';
var GDP_DIR = './';
var GDPJS_DIR = './';
// FIXME: Don't hardwire this.
var NODE_MODULES_DIR = '/Users/cxh/ptII/vendors/node_modules/';

var gdpjsSupport = require(LIBGDP_H_DIR + 'gdpjs.js');

exports.ep_dbg_init = gdpjsSupport.ep_dbg_init;

exports.gclPrintableNameType = gdpjsSupport.gcl_pname_t;

/**
 * Open a Global Data Plane log.
 * @param name The external, possibly human readable name.
 * @param mode The mode (0: read only, 1: write only, 2: read/write)
 * @return the GCL handle;
 */
exports.gdp_gcl_open = function (name, iomode) {
    console.log("gdpjs/index.js: gdp_gcl_open(" + name + ", " + iomode + ")");

    console.log("gdpjs/index.js: gdp_gcl_open(" + name + ", " + iomode + "): calling gdp_init_js()");

    // FIXME: Need to figure out how to allocate ebuf.
    // One issue is that we don't want this to go out of scope.

    var ebuf = ref.allocCString('123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890');

    var gdpdAddress = null;
    estat = gdpjsSupport.gdp_init_js( /* String */ gdpdAddress);
    if ( ! gdpjsSupport.ep_stat_isok(estat) ) {
        var message = "gdpjs/index.js: gdp_gcl_open_js(): " +
            " Failed to initialize with address " + gdpdAddress +
            " " + gdpjsSupport.ep_stat_tostr(estat, ebuf, ebuf.length);
        console.log(message);
        // FIXME: Need an error output
        throw new Error(message);
    }

    console.log("gdpjs/index.js: gdp_gcl_open(" + name + ", " + iomode + "): calling gdp_gcl_open()");
    var rv = gdpjsSupport.gdpGclOpen(name, iomode);
    var estat = rv.error_code;
    gclH = rv.gclH;
    if ( ! gdpjsSupport.ep_stat_isok(estat) ) {
        var message = "gdp.js: GDP(" + name + ", " + iomode + "): gdp_init_js() returned not ok: "
            + gdpjsSupport.ep_stat_tostr(estat, ebuf, ebuf.length);
        console.log(message);
        // FIXME: Need an error output
        throw new Error(message);
    }
    return gclH;
}

/**
 * @param gdpdAddress  gdp daemon's <host:port>; if null, use default "127.0.0.1:2468"
 * @param gclName      name of existing GCL 
 * @param firstRecord  The first record number to be read
 * @param numberOfRecords The number of records to read.
 * TBD recdest is not used anymore
 * recdest = -1  writes the gcl records to stdout with readable formatting
 * recdest =  0  read the gcl records into the return value's Array { records: }
 * conout        Boolean
 * Iff recdest == 0 and conout == true; the Array entries written to the gcl
 * will also be echoed to console.log().  The other recdest destinations will
 * ALL result in console.log() output; conout is ignored.
 * Note, there still may be undesired output via console.log() and
 * console.error(). TBD
*/
exports.read_gcl_records = function (gdpdAddress, gclName,
                                   firstRecord, numberOfRecords,
                                   subscribe, multiread, recordDestination,
                                   consoleOut, eventCallBackFunction,
                                   waitForEvents) {

    return gdpjsSupport.read_gcl_records(gdpdAddress, gclName,
                         firstRecord, numberOfRecords,
                         subscribe, multiread, recordDestination,
                         consoleOut, eventCallBackFunction,
                         waitForEvents);
}

/** Write to a Global Data Plane log.
 *  
 * @param gdpdAddress gdp daemon's <host:port>; if null, use default
 * "127.0.0.1:2468" @param gclName name of existing GCL @param
 * logdxname String: the name of the log server.  Use os.hostname()
 * for local @param gclAppend Boolean: append to an existing GCL
 *
 * @param recordSource The source of the records. recordSource == -1:
 * read the gcl records to be written from stdin with prompts to and
 * echoing for the user on stdout. recordSource = 0: read the gcl
 * records from the Array recarray In this case only, for each gcl
 * record written we will return in the parallel array recarray_out: {
 * recno: Integer, time_stamp: <timestamp_as_String> }. Note,
 * recarray_out must be in the incoming parameter list. recsrc > 0
 * write recsrc records with automatically generated content: the
 * integers starting at 1 and going up to recsrc, inclusive.
 *
 * @param recordArray if recordSource is 0, then the records are written to 
 * recordArray.
 *
 * @param consoleOut Boolean: iff true, for recsrc = -1, prompt user
 * and echo written records on stdout; for recsrc = 0, echo written
 * records on stdout, not recommended; for recsrc > 0, Note, echoed
 * written records also include GCL record number (recno) and
 * timestamp.
 *
 * @param recordArrayOut Array: see recsrc = 0, above.
 */
exports.write_gcl_records = function (gdpdAddress, gclName, logdxname, gclAppend,
                                      recordSource, recordArray, consoleOut,
                                      recordArrayOut) {
    return gdpjsSupport.write_gcl_records(gdpdAddress, gclName, logdxname, gclAppend,
                                   recordSource, recordArray, consoleOut,
                                   recordArrayOut);
}

