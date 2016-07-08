/* vim: set ai sw=4 sts=4 ts=4 : */

// JavaScript support routines for GDP access from Node.js-based JavaScript
// 2014-11-02
// Alec Dara-Abrams

// TBD: put this code into a Node.js module; add Copyrights


// JS wrappers for calls to libep, libgdp, and libgdpjs FFI functions =======

// Currently, we are using an eval-based mechanism to include the above
// libraries as well as this code; we hope to construct a Node.js module
// to better encapsulate all these support functions and variables.


/* Note:
   EP_STAT      JS<==>C  uint32
   gclHandle_t  JS<==>C  gdp_gcl_t *
   String       JS<==>C  gcl_name_t *
   TBD -- get these JS<-->C type correspondences documented here
   and in each ...._js() function below.
*/


/* EP_STAT */
function ep_stat_ok_js()
{
    return libgdpjs.ep_stat_ok();
}

/* C: size_t  <==> JS: Integer */
function sizeof_EP_STAT_in_bytes_js()
// Pull libgdp/libep information into JS.
// Really, a C compile-time constant we bring into JS
{
    return libgdpjs.sizeof_EP_STAT_in_bytes();
}

/* EP_STAT */
function gdp_init_js( /* String */ gdpd_addr )
{
    // libgdp.gdp_init() seems to have trouble passing in an explicit
    // null string but an undefined argument works OK to start the
    // library on the default gdpd host:port .
    // So we protect this call from incoming JS null strings.
    if ( gdpd_addr == "" )
	{	// DEBUG: console.log( 'then: gdpd_addr = \"' + gdpd_addr + '\"' );
            return libgdp.gdp_init( undefined );
	}
    else
	{	// DEBUG: console.log( 'else: gdpd_addr = \"' + gdpd_addr + '\"' );
            return libgdp.gdp_init( gdpd_addr );
	}
}

/* Boolean */
function ep_stat_isok_js( /* EP_STAT */ estat )
{
    return libgdpjs.ep_stat_isok(estat);
}

/* EP_STAT */
function gdp_stat_nak_notfound_js()
// return a constant "NAK NOTFOUND" EP_STAT value
{
    return libgdpjs.gdp_stat_nak_notfound();
}

/* EP_STAT */
function ep_stat_end_of_file_js()
// return a constant "END OF FILE" EP_STAT value
{
    return libgdpjs.ep_stat_end_of_file();
}

/* Boolean */
function ep_stat_is_same_js( estat_a, estat_b )
{
    return libgdpjs.ep_stat_is_same( estat_a, estat_b );
}

/* { error_code: EP_STAT, gclH: gclHandle_t }; */
function gdp_gcl_create_js( /* gcl_name_t */ gcliname,
        gclPtrPtr
                            )
{
    var gcl_Ptr;                                  // gclHandle_t 
    var gclPtrPtr = ref.alloc( gdp_gcl_tPtrPtr ); // gclHandle_t *
    var estat = libgdp.gdp_gcl_create(gcliname, gclPtrPtr);
    gcl_Ptr = gclPtrPtr.deref();
    return { error_code: estat, gclH: gcl_Ptr };
}

/* EP_STAT */
function gdp_parse_name_js( /* String */ xname, /* gcl_name_t */ gcliname )
{
    return libgdp.gdp_parse_name( xname, gcliname );
};

/* { error_code: EP_STAT, gclH: gclHandle_t }; */
function gdp_gcl_open_js( /* gcl_name_t */   gcliname,
        /* gdp_iomode_t */ mode,
        gclPtrPtr
                          )
{
    var gcl_Ptr;                                  // gclHandle_t 
    var gclPtrPtr = ref.alloc( gdp_gcl_tPtrPtr ); // gclHandle_t *
    var estat = libgdp.gdp_gcl_open(gcliname, mode, ref.NULL, gclPtrPtr);
    gcl_Ptr = gclPtrPtr.deref();
    return { error_code: estat, gclH: gcl_Ptr };
}

/* void */
function ep_dbg_init_js( )
{
    libep.ep_dbg_init( );
};

/* String */
function ep_stat_tostr_js( /* EP_STAT */ estat,
        /* String */  str,
        /* Integer */ str_len
                           )
{
    return libep.ep_stat_tostr(estat, str, str.length);
};

/* EP_STAT */
function ep_time_now_js( /* EP_TIME_SPEC * */ EP_TIME_SPEC_struct_Ptr )
{
    return libep.ep_time_now( EP_TIME_SPEC_struct_Ptr );
};

/* void */
function ep_dbg_set_js( /* String */  ep_dbg_pattern )
{
    libep.ep_dbg_set( ep_dbg_pattern );
};

/* String */
function gdp_printable_name_js( /* gcl_name_t  */ gclname,
        /* gcl_pname_t */ gclpname )
{
    var rv_str;
    rv_str = libgdp.gdp_printable_name( gclname, gclpname );
    return rv_str;
}

/* void */
function gdp_gcl_print_stdout_js( gcl_Ptr )
{
    /* void */
    libgdpjs.gdp_gcl_print_stdout( gcl_Ptr );
}

/* String */
function gdp_get_pname_from_gclh_js( gcl_Ptr )
{
    var rv_str;
    rv_str = libgdpjs.gdp_get_pname_from_gclh( gcl_Ptr );
    return rv_str;
}

/* String */
function gdp_get_printable_name_from_gclh_js( gcl_Ptr )
{
    var rv_str;
    // Note, the below returns a pointer to a statically allocated gcl_pname_t,
    // currently, typedef char gcl_pname_t[GDP_GCL_PNAME_LEN + 1] in gdp.h
    rv_str = libgdpjs.gdp_get_printable_name_from_gclh( gcl_Ptr );
    return rv_str;
}

/* String */
function ep_time_as_string_js( ep_time_spec, /* Boolean */ human_format )
{
    var rv_str;
    // Note, the below returns a pointer to a statically allocated 
    // char tbuf[100] in gdpjs_supt.c/ep_time_as_string() .
    rv_str = libgdpjs.ep_time_as_string( ep_time_spec, human_format );
    // DEBUG console.log("ep_time_as_string_js: rv_str = '" + rv_str + "'" );
    return rv_str;
}

/* String */
function gdp_datum_getts_as_string_js( datum, /* Boolean */ human_format )
{
    var rv_str;
    // Note, the below returns a pointer to a statically allocated 
    // char tbuf[100] in gdpjs_supt.c/ep_time_as_string() .
    rv_str = libgdpjs.gdp_datum_getts_as_string( datum, human_format );
    // DEBUG console.log("gdp_datum_getts_as_string_js: rv_str = '" + rv_str + "'" );
    return rv_str;
}

/* datum */
function gdp_datum_new_js()
{
    return libgdp.gdp_datum_new();

};

/* EP_STAT */
function gdp_gcl_read_js( gclh, recno, datum )
{
    return libgdp.gdp_gcl_read( gclh, recno, datum );
}

/* gdp_recno_t */
function gdp_datum_getrecno_js( datum )
{
    return libgdp.gdp_datum_getrecno( datum );
}

/* EP_STAT */
function gdp_gcl_publish_buf_js( gcl_Ptr, datum, buf )
{
    var rv_estat;
    var temp_gdp_buf = libgdp.gdp_datum_getbuf( datum );
    libgdp.gdp_buf_write( temp_gdp_buf, buf, buf.length );
    rv_estat = libgdp.gdp_gcl_publish( gcl_Ptr, datum );
    return rv_estat;
};

// TBD: WARNING - likely will JS blow up at FFI  LEFT OFF HERE
/* EP_STAT */
function gdp_gcl_subscribe_no_callback_js( gclh, firstrec, numrecs,
        timeout /* seconds */ )
// This is a limited-functionality version of
//   libgdp.gdp_gcl_subscribe(gclh, firstrec, numrecs, timeout, cbfunc, cbarg);
// See the comments on callbacks below for:
//    gdp_gcl_subscribe_no_timeout_no_callback_js() .
// A smaller issue is passing in the EP_TIME_SPEC timeout argument (a C struct).
// Node.js ref-struct should provide a straight-forward FFI implementation.
// This smaller issue is also TBD.
{
    var rv_estat;
    // EP_TIME_SPEC *timeout, gdp_gcl_sub_cbfunc_t cbfunc, void *cbarg 
    // are all null here for a simpler subscribe variant.  See gdp/gdp-api.[ch]
    // gdp_gcl_subscribe() for details.
    // TBD: this use of timeout should blow up at the FFI - time to bite the
    //      bullet and use ref-struct ?
    rv_estat = libgdp.gdp_gcl_subscribe( gclh, firstrec, numrecs,
            timeout, null, null );
    return rv_estat;
};

/* EP_STAT */
function gdp_gcl_subscribe_no_timeout_no_callback_js( gclh, firstrec, numrecs )
// This is a limited-functionality version of
//   libgdp.gdp_gcl_subscribe(gclh, firstrec, numrecs, timeout, cbfunc, cbarg);
// We currently do not have a mechanism for invoking a JS callback function
// from down in the C level.  We are looking for a solution here using the
// Node.js FFI mechanism. TBD
// A smaller issue is passing in the EP_TIME_SPEC timeout argument (a C struct).
// Node.js ref-struct should provide a straight-forward FFI implementation.
// This smaller issue is also TBD.
{
    var rv_estat;
    // EP_TIME_SPEC *timeout, gdp_gcl_sub_cbfunc_t cbfunc, void *cbarg 
    // are all null here for a simpler subscribe variant.  See gdp/gdp-api.[ch]
    // gdp_gcl_subscribe() for details.
    rv_estat = libgdp.gdp_gcl_subscribe( gclh, firstrec, numrecs,
            null, null, null );
    return rv_estat;
};

/* EP_STAT */
function gdp_gcl_multiread_no_callback_js( gclh, firstrec, numrecs )
// This is a limited-functionality version of
//   libgdp.gdp_gcl_multiread(gclh, firstrec, numrecs, cbfunc, cbarg);
// See the comments on callbacks above for:
//    gdp_gcl_subscribe_no_timeout_no_callback_js() .
{

    var rv_estat;
    // gdp_gcl_sub_cbfunc_t cbfunc, void *cbarg are both null here for a
    // simpler multiread variant.  See gdp/gdp-api.[ch] gdp_gcl_multiread() .
    rv_estat = libgdp.gdp_gcl_multiread( gclh, firstrec, numrecs,
            null, null);
    return rv_estat;
}

/* C: gdp_event_t *  <==>  node-ffi: gdp_event_tPtr  <==>  JS: Object */
function gdp_event_next_js( /* Boolean */ wait )
// Return value can be viewed by JS as an opaque handle for a gdp_event.
// Returns null iff there is no event pending when wait == true.
{
    var gev_Ptr;
    gev_Ptr = libgdp.gdp_event_next( wait );
    return gev_Ptr;
}

/* Integer */
function gdp_event_gettype_js( gev_Ptr )
// Arg gev_Ptr can be viewed by JS as an opaque handle for a gdp_event.
// Return value is an integer gdp event code -- see libgdp_h.js.
// If gev_Ptr == null, return null.  TBD: what if gev_Ptr is not present?
{
    var evtype_int;
    if ( gev_Ptr == null ) return null;  // protect libgdp. call below
    // Note, if gev_Ptr == null, fails libgdp assertion with a runtime Abort.
    evtype_int = libgdp.gdp_event_gettype( gev_Ptr );
    return evtype_int
        }

/* C: gdp_gcl_t *  <==>  JS: Object */
function gdp_event_getgcl_js( gev_Ptr )
// Arg gev_Ptr can be viewed by JS as an opaque handle for a gdp_event.
// Return value can be viewed by JS as an opaque handle for a gdp_gcl.
// If gev_Ptr == null, return null.  TBD: what if gev_Ptr is not present?
{
    if ( gev_Ptr == null ) return null;  // protect libgdp. call below
    // Note, if gev_Ptr == null, fails libgdp assertion with a runtime Abort.
    return libgdp.gdp_event_getgcl( gev_Ptr );
}

/* C: gdp_datum_t *  <==>  JS: Object */
function gdp_event_getdatum_js( gev_Ptr )
// Arg gev_Ptr can be viewed by JS as an opaque handle for a gdp_event.
// Return value can be viewed by JS as an opaque handle for a gdp_datum.
// If gev_Ptr == null, return null.  TBD: what if gev_Ptr is not present?
{
    if ( gev_Ptr == null ) return null;  // protect libgdp. call below
    // Note, if gev_Ptr == null, fails libgdp assertion with a runtime Abort.
    return libgdp.gdp_event_getdatum( gev_Ptr );
}

/* void */
function gdp_datum_print_stdout_js( datum )
// Arg datum can be viewed by JS as an opaque handle for a gdp_datum.
{
    /* void */
    libgdpjs.gdp_datum_print_stdout( datum );
}


/* EP_STAT */
function gdp_event_free_js( gev_Ptr )
// Arg gev_Ptr can be viewed by JS as an opaque handle for a gdp_event.
{
    return libgdp.gdp_event_free( gev_Ptr );
}

/* EP_STAT */
function gdp_gcl_close_js( gcl_Ptr )
{
    return libgdp.gdp_gcl_close(gcl_Ptr);
}

/* void */
function fflush_all_js()
// Explicit print buffer flush; sometimes Node.js may not empty buffers
{
    /* void */
    libgdpjs.fflush_all();
}

/* void */
function gdp_datum_free_js( datum )
{
    /* void */
    libgdp.gdp_datum_free( datum );
}



// Some GDP-related utility string manipulation functions ===================


/* Integer */
function String_to_Int( /* String */ str )
{
    if ( /^(\-|\+)?([0-9]+|Infinity)$/ .test(str) ) return Number(str);
    return NaN;
}


/* String */
function array_to_escaped_String( /* Array */ arry )
// Helpful for printing SHA-256 internal gcl names.
{
    var iToH = [ "0", "1", "2", "3", "4", "5", "6", "7",
            "8", "9", "A", "B", "C", "D", "E", "F" ];
    var arry_as_string = "";
    for( var i = 0; i < arry.length; i++ )
	{
            var hiHexDigit = iToH[ Math.floor( arry[i] / 16 ) ];
            var loHexDigit = iToH[ arry[i] % 16 ];
            arry_as_string = arry_as_string + jsesc( String.fromCharCode( arry[i] ) );
            // console.log( "array_to_escaped_String: ", i, arry[i],
            //              hiHexDigit, loHexDigit, arry_as_string ); 
	}
    return arry_as_string;
}


/* String */
function array_to_String( /* Array */ arry )
// Helpful for printing SHA-256 internal gcl names.
{
    var iToH = [ "0", "1", "2", "3", "4", "5", "6", "7",
            "8", "9", "A", "B", "C", "D", "E", "F" ];
    var arry_as_string = "";
    for( var i = 0; i < arry.length; i++ )
	{
            var hiHexDigit = iToH[ Math.floor( arry[i] / 16 ) ];
            var loHexDigit = iToH[ arry[i] % 16 ];
            arry_as_string = arry_as_string + String.fromCharCode( arry[i] );
            // console.log( "array_to_String: ", i, arry[i],
            //              hiHexDigit, loHexDigit, arry_as_string ); 
	}
    return arry_as_string;
}


/* String */
function array_to_HexDigitString( /* Array */ arry )
// Helpful for printing SHA-256 internal gcl names.
{
    var iToH = [ "0", "1", "2", "3", "4", "5", "6", "7",
            "8", "9", "A", "B", "C", "D", "E", "F" ];
    var arry_as_hex = "";
    for( var i = 0; i < arry.length; i++ )
	{
            var hiHexDigit = iToH[ Math.floor( arry[i] / 16 ) ];
            var loHexDigit = iToH[ arry[i] % 16 ];
            arry_as_hex = arry_as_hex + hiHexDigit + loHexDigit;
            // console.log( "array_to_HexDigitString: ", i, arry[i],
            //              hiHexDigit, loHexDigit, arry_as_hex ); 
	}
    return arry_as_hex;
}


/* String */
function buffer_to_HexDigitString( /* Buffer */ bfr )
// Helper function for buffer_info_to_string() .
{
    var iToH = [ "0", "1", "2", "3", "4", "5", "6", "7",
            "8", "9", "A", "B", "C", "D", "E", "F" ];
    var bfr_as_hex = "";
    for( var i = 0; i < bfr.length; i++ )
	{
            var hiHexDigit = iToH[ Math.floor( bfr[i] / 16 ) ];
            var loHexDigit = iToH[ bfr[i] % 16 ];
            bfr_as_hex = bfr_as_hex + hiHexDigit + loHexDigit;
            // console.log( "buffer_to_HexDigitString: ", i, bfr[i],
            //              hiHexDigit, loHexDigit, bfr_as_hex ); 
	}
    return bfr_as_hex;
}


/* String */
function buffer_info_to_string( /* string */ bfrName, /* Buffer */ bfr )
// Used for debugging node-ref "types" which may be implemented as a JS Buffer.
{
    var isNonNullObject = ( bfr != null ) && ( typeof(bfr) == 'object' );
    var str = "";
    // console.log( "buffer_info_to_string: " );
    // console.log( "isNonNullObject =\"" + isNonNullObject + "\"" );
    // console.log( "bfr =\"" + bfr + "\"" );
    // console.log( "typeof(bfr) =\"" + typeof(bfr) + "\"" );
    //if ( typeof(bfr) == 'object' && bfr.constructor.name == "Buffer" )
    if ( isNonNullObject )
	{
            if ( bfr.constructor.name == "Buffer" )
		{
                    str = str + bfrName + " =\"" + bfr + "\"\n";
                    str = str + "typeof(" + bfrName + ") =\"" + typeof(bfr) + "\"\n";
                    str = str + bfrName + ".constructor.name =\"" + 
                        bfr.constructor.name + "\"\n";
                    str = str + bfrName + ".length =\"" + bfr.length + "\"\n";
                    str = str + "buffer_to_HexDigitString(" + bfrName + ") =\"" +
                        buffer_to_HexDigitString( bfr ) + "\"\n";
                    str = str + bfrName + ".toString() =\"" + bfr.toString() + "\"\n";
                    str = str + "ref(" + bfrName + ").getType =\"" +
                        ref.getType(bfr) + "\"\n";
                    str = str + bfrName + ".deref =\"" + bfr.deref() + "\"";
		} else
		{
                    str = "\"" + bfrName + "\" is not a Buffer object, it is a \"" +
                        bfr.constructor.name + "\"";
		}
	} else
	{
            str = "\"" + bfrName +
                "\" is not a non-null Buffer object, typeof = \"" +
                typeof( bfr ) + "\", value = \"" + bfr + "\"" ;
	}
    return str;
} /* end function buffer_info_to_string() */


/* String */
function get_datum_buf_as_string( datum )
// Return the bytes of a GDP datum as a JS String (which is able to
// contain nulls).
{
    var datum_dlen = libgdp.gdp_datum_getdlen( datum );

    // DEBUG - keep this output here until we've looked this code over more.
    DEBUG_get_datum_buf_as_string = false;

    if ( DEBUG_get_datum_buf_as_string ) {
	console.log( "get_datum_buf_as_string: After call to gdp_datum_print() gcp_datum_getdlen() = \"" + datum_dlen + "\"" );
    }
    var gdp_buf = libgdp.gdp_datum_getbuf( datum );
    if ( DEBUG_get_datum_buf_as_string ) {
	console.log( "get_datum_buf_as_string: After call to gdp_datum_getbuf() gcp_datum_getbuf() = \"" + gdp_buf + "\"" );
    }
    // curItem will hold the contents of the GCL item to be read here
    var curItem   = new buf_t(1000); // TBD check size??
    var gdp_buf_size = libgdp.gdp_buf_read( gdp_buf, curItem, curItem.length );
    if ( DEBUG_get_datum_buf_as_string ) {
	console.log( "get_datum_buf_as_string: After call to gdp_buf_read() gdp_buf_read() = \"" + gdp_buf_size + "\"" );
	console.log( "get_datum_buf_as_string: After call to gdp_buf_read() curItem = \"" + curItem.toString() + "\"\n" );
    }
    var rvString = '';
    for ( var i = 0; i < gdp_buf_size; i++ )
	{  rvString = rvString + String.fromCharCode( curItem[i] );
	}
    if ( DEBUG_get_datum_buf_as_string ) {
	console.log("get_datum_buf_as_string: rvString = '" + rvString + "'" );
    }
    return rvString;
} /* function get_datum_buf_as_string() */



// Some temporary test output ===============================================

// We usually call this test routine just after we set up the server in the 
// Node.js global scope.  Eyeball the console.log() output to check the tests.
// This code also serves as temporary examples of use.
// If still useful, we will move this code to a real test routine.

function misc_lower_level_inline_tests( /* Boolean */ do_tests )
{
    /* Simple tests of:
       libep.ep_time_accuracy()
       libep.ep_time_now()
       ep_time_as_string_js()
       ep_time_now_js()
       and EP_TIME_SPEC_struct to pass arguments in and out.
       Write to console.log().
    */

    if ( ! do_tests ) return;

    console.log( 'A:' );
    var tacc /* float */  = libep.ep_time_accuracy();
    console.log( 'ep_time_accuracy() = ' + tacc );

    var eptspec = new EP_TIME_SPEC_struct;
    estat  = libep.ep_time_now( eptspec.ref() );
    console.log( 'eptspec.tv_sec = \"' + eptspec.tv_sec + '\"' );
    console.log( 'eptspec.tv_nsec = \"' + eptspec.tv_nsec + '\"' );
    console.log( 'eptspec.tv_accuracy = \"' + eptspec.tv_accuracy + '\"' );

    // timebuf will hold the EP_TIME_SPEC we read above in String format.
    var timebuf = ep_time_as_string_js( eptspec.ref(), true );
    console.log( 'timebuf = \"' + timebuf + '\"' );

    // to slow things down to be able to follow console.log()
    // sleep.sleep(5);
    console.log( 'B:' );
    estat  = ep_time_now_js( eptspec.ref() );
    console.log( 'eptspec.tv_sec = \"' + eptspec.tv_sec + '\"' );
    console.log( 'eptspec.tv_nsec = \"' + eptspec.tv_nsec + '\"' );
    console.log( 'eptspec.tv_accuracy = \"' + eptspec.tv_accuracy + '\"' );

    var timebuf = ep_time_as_string_js( eptspec.ref(), true );
    console.log( 'timebuf = \"' + timebuf + '\"' );


    // DEBUG
    // Tests of setTimeout() and setInteral() - remove when gdpREST_server.js
    // subscription has been proven.

    if ( false )
        {
            // The only way out now is for the Node.js event loop to terminate.
            // For now, this is by a ^C from the terminal it was started in or some
            // other process signal.

            var delay_Tout = 5000;  // milliseconds
            var aTimeoutObject;
            var nCalls_cbf_Tout = 0;

            function cbf_Tout( a1, a2 ) 
            {
                nCalls_cbf_Tout++;
                console.log( 'In cbf_Tout nCalls_cbf_Tout = %d', nCalls_cbf_Tout );
                console.log( 'a1 = \"' + a1 + '\"' );
                console.log( 'a2 = \"' + a2 + '\"' );
                clearInterval( anIntervalObject );
            }

            var cbf_Tout_arg_1 = 1, cbf_Tout_arg_2 = 2;

            // setTimeout(callback, delay, [arg], [...])
            aTimeoutObject = setTimeout( cbf_Tout, delay_Tout, 
                    cbf_Tout_arg_1, cbf_Tout_arg_2 );

            var delay_Int = 1000;  // milliseconds
            var anIntervalObject;
            var nCalls_cbf_Int = 0;

            function cbf_Int( a1, a2 ) 
            {
                nCalls_cbf_Int++;
                console.log( 'In cbf_Int nCalls_cbf_Int = %d', nCalls_cbf_Int );
                console.log( 'a1 = \"' + a1 + '\"' );
                console.log( 'a2 = \"' + a2 + '\"' );
            }

            var cbf_Int_arg_1 = 1, cbf_Int_arg_2 = 2;

            // setInterval(callback, delay, [arg], [...])
            anIntervalObject = setInterval( cbf_Int, delay_Int,
                    cbf_Int_arg_1, cbf_Int_arg_2 );


            // clearTimeout( aTimeoutObject );

            // clearInterval( anIntervalObject );
        } /* end if ( false ) */


    if ( false )
        {
            // var timeout  = 20;  // seconds
            var timeout  = 5;  // seconds
            // will be set true after timeout seconds by cbf_Tout() as a result of
            // setTimeout( cbf_Tout, ...).  We stop polling below when it goes true.
            var monitoring_done = false;
            console.log( 'monitoring_done = \"' + monitoring_done + '\"' );

            // var delay_Tout = 5000;  // milliseconds
            var delay_Tout = timeout * 1000;  // seconds to milliseconds
            var aTimeoutObject;
            var nCalls_cbf_Tout = 0;

            function cbf_Tout( a1, a2 ) 
            {
		nCalls_cbf_Tout++;
		console.log( 'In cbf_Tout nCalls_cbf_Tout = %d', nCalls_cbf_Tout );
		console.log( 'a1 = \"' + a1 + '\"' );
		console.log( 'a2 = \"' + a2 + '\"' );
		fflush_all_js();  // sometimes Node.js may not empty buffers
		clearInterval(anIntervalObject)
                    monitoring_done = true;
		console.log( 'monitoring_done = \"' + monitoring_done + '\"' );
            }

            var cbf_Tout_arg_1 = 1, cbf_Tout_arg_2 = 2;

            // setTimeout(callback, delay, [arg], [...])
            aTimeoutObject = setTimeout( cbf_Tout, delay_Tout, 
                    cbf_Tout_arg_1, cbf_Tout_arg_2 );

            var delay_Int = 1000;  // milliseconds
            var anIntervalObject;
            var nCalls_cbf_Int = 0;

            function cbf_Int( a1, a2 ) 
            {
                nCalls_cbf_Int++;
                console.log( 'In cbf_Int nCalls_cbf_Int = %d', nCalls_cbf_Int );
                console.log( 'a1 = \"' + a1 + '\"' );
                console.log( 'a2 = \"' + a2 + '\"' );
                {	console.log( 'Do a loop' ); }
            }

            var cbf_Int_arg_1 = 1, cbf_Int_arg_2 = 2;

            // setInterval(callback, delay, [arg], [...])
            anIntervalObject = setInterval( cbf_Int, delay_Int,
                    cbf_Int_arg_1, cbf_Int_arg_2 );


            // while ( ! monitoring_done )
            // {	console.log( 'Do a loop' ); }
            console.log( 'Done looping' );

        } /* end if ( false ) */



} /* end function misc_lower_level_inline_tests() */

// ==========================================================================

