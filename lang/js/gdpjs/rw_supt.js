/* vim: set ai sw=4 sts=4 ts=4 : */

// Node.js Javascript support routines for reading and writing gcl's
//
// Alec Dara-Abrams
// 2014-11-05
//
// TBD: Copyright, clean up code; bring internal doc up to date;
//      regularize indenting with JS-aware tool
//      Check for possible error returns from libgdp calls - see TBD1 .
//
// Used by:
//  writer-test.js    -- Node.js Javascript version of gdp/apps/writer-test.c
//  reader-test.js    -- Node.js Javascript version of gdp/apps/reader-test.c
//  gdpREST_server.js -- Node.js-Based JavaScript server to provide a
//                       REST interface to the GDP


// ========================================================================
// Example set ups for calls to write_gcl_records()
//
// A: write to gcl from stdin
// recsrc   = -1;   // read from stdin the gcl records to be written,
//                  // with optional prompts to and echoing for the
//                  // user on stdout.
// conout   = true; // do prompt and echo to stdout.
// recarray = [ ]; recarray_out = []; // ignored for recsrc = -1
//
// B: write to gcl from JS Array, recarray[]
// recsrc   =  0;      // read the gcl records from the Array recarray[]
// recarray = [ "Item 01 - from recarray", "Item 02", "Item 03" ];
// conout   = false;   // don't echo to console.log()
// recarray_out = [];  // will hold recno's and timestamps for newly
//                     // written records
//
// C: write to gcl N records with integers as contents
// recsrc >  0;     // write recsrc records with automatically generated
//                  // content: the integers starting at 1 and going
//                  // up to recsrc, inclusive.
// recsrc =  7;
// conout = false;  // don't echo to console.log()
// recarray = [ ]; recarray_out = []; // ignored for recsrc > 0
//
// write_gcl_records( gdpd_addr, gcl_name, gcl_append, recsrc,
//                    recarray, conout, recarray_out
//                  );

/*  Returns:
    { error_isok: false|true, error_code: EP_STAT, error_msg: String,
    gcl_name: String
    }
*/
function write_gcl_records( gdpd_addr, gcl_name, gcl_append, 
        recsrc, recarray, conout, recarray_out )

// gdpd_addr    gdp daemon's <host:port>; if null, use default "127.0.0.1:2468"
// gcl_name     if gcl_append is true, name of existing GCL; 
//              if gcl_append is false, ignored.  A new GCL will be created.
// gcl_append   Boolean: append to an existing GCL
// recsrc = -1  read the gcl records to be written from stdin with
//              prompts to and echoing for the user on stdout
// recsrc =  0  read the gcl records from the Array recarray
//              In this case only, 
//              For each gcl record written we will return in the parallel array
//              recarray_out:
//                 { recno: Integer, time_stamp: <timestamp_as_String> }.
//              Note, recarray_out must be in the incoming parameter list above.
// recsrc >  0  write recsrc records with automatically generated
//              content: the integers starting at 1 and going up to
//              recsrc, inclusive.
// conout       Boolean: iff true,
//              for recsrc = -1, prompt user and echo written records on stdout;
//              for recsrc = 0, echo written records on stdout, not recommended;
//              for recsrc > 0,  "
//              Note, echoed written records also include GCL record number
//              (recno) and timestamp.
// recarray_out Array: see recsrc = 0, above.
//
// TBD: Note, there still may be undesired output via console.log() and
// console.error(). Check all uses of  if ( conout == true ) below.
// TBD: We could also return recarray_out[] for recsrc > 0. And, even
// augmented with the manually entered record content, for recsrc = -1.
{	
    // internal variables for historical reasons
    var xname   = gcl_name;
    var append  = gcl_append;
    var numrecs = recsrc;

    // Local working variables
    var gcl_Ptr; // gclh
    var estat;   // EP_STAT
    var gcliname = ref.alloc(gcl_name_t);
    // Note, buf is re-allocated below for each item string read from stdin
    var buf      = new buf_t(10);


    estat = gdp_init_js( /* String */ gdpd_addr );
    // TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

    if ( conout == true )
	{	// allow thread to settle to avoid interspersed debug output
		// TBD check this
            sleep.sleep(1); // needed only for stdout and stderr
	}

    if ( xname == null )
	{
            // TBD: check signature
            var gclcreaterv = gdp_gcl_create_js( null );
            estat   = gclcreaterv.error_code;
            gcl_Ptr = gclcreaterv.gclH;
            // grab the name of the newly created gcl
            gcl_name = gdp_get_pname_from_gclh_js( gcl_Ptr );
	}
    else
	{
            gdp_parse_name_js( xname, gcliname );

            // TBD: especially check for gcliname already existing??!!
	
            if (append)
		{
                    // TBD: check signature
                    var gclopenrv = gdp_gcl_open_js(gcliname, GDP_MODE_AO );
                    estat   = gclopenrv.error_code;
                    gcl_Ptr = gclopenrv.gclH;
		}
            else
		{
                    // TBD: check signature
                    var gclcreaterv = gdp_gcl_create_js( gcliname );
                    estat   = gclcreaterv.error_code;
                    gcl_Ptr = gclcreaterv.gclH;
		}
	}
    // TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

    // don't always send gcl name to stdout
    if ( conout == true )
	{	gdp_gcl_print_stdout_js( gcl_Ptr ); }

    var datum;
    datum = gdp_datum_new_js();
		
    if ( numrecs < 0 )
	{
            // Read records from stdin, prompting & echoing to user on stdout

            if ( conout == true )  // TBD is if(conout) is correct here?
		{ console.log( "\nStarting to read input - ^D to end" ); }
            var rvget;  /* String */
            // really a dummy for gets's parameter; we ignore its value.
            // Just trying to avoid possible buffer overruns inside gets().
            var strbuf = new Array( 200 + 1 ).join( " " );  // long enough??

            while ( (rvgets = libc.gets( strbuf ) ) != null )
		{
                    var buf   = new buf_t(rvgets.length+1);  // we'll tack on a \0
                    for ( var i = 0; i < rvgets.length; i++ )
			{  buf[i] = rvgets.charCodeAt(i);  // not sure if really necessary
			}
                    buf[rvgets.length] = 0;  // Hopefully, interpreted in C as \0
                    if ( conout == true )  // TBD is if(conout) is correct here?
			{ console.log( "Got input %s%s%s", "<<", rvgets, ">>" ); }

                    estat = gdp_gcl_publish_buf_js( gcl_Ptr, datum, buf );
                    // TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

                    if ( conout == true )  // TBD is if(conout) is correct here?
			{ gdp_datum_print_stdout_js( datum ); }

		} /* end while */
	}
    else if ( numrecs > 0 )
	{
            // Generate numrecs records with contents = integers 1 to numrecs.

            // For each gcl record written we will return in the parallel array
            // recarray_out:
            //    { recno: Integer, time_stamp: <timestamp_as_String> }.
            // Note, recarray_out must be in the incoming parameter list above.

            for ( var crec = 1; crec <= numrecs; crec++ )
		{
                    var rvgets;  /* String */
                    rvgets = crec.toString();

                    var buf   = new buf_t(rvgets.length+1);  // we'll tack on a \0
                    for ( var i = 0; i < rvgets.length; i++ )
			{  buf[i] = rvgets.charCodeAt(i);  // not sure if really necessary
			}
                    buf[rvgets.length] = 0;  // Hopefully, interpreted in C as \0
                    if ( conout == true )  // TBD is if(conout) is correct here?
			{ console.log( "Got input %s%s%s", "<<", rvgets, ">>" ); }

                    estat = gdp_gcl_publish_buf_js( gcl_Ptr, datum, buf );
                    // TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

                    // grab record number and timestamp for this newly written record
                    var ts = gdp_datum_getts_as_string_js( datum, true /* format */ );
                    // TBD: below check for 64-bit integer return type, gdp_recno_t
                    var rn = gdp_datum_getrecno_js( datum );
                    recarray_out[crec-1] = { recno: rn, time_stamp: ts };

                    if ( conout == true )  // TBD is if(conout) is correct here?
			{ gdp_datum_print_stdout_js( datum ); }
		} /* end for ( var crec = 1 ...) */
	}
    else  // numrecs == 0
	{
            // Write contents of recarray[] to the gcl

            // For each gcl record written we will return in the parallel array
            // recarray_out:
            //    { recno: Integer, time_stamp: <timestamp_as_String> }.
            // Note, recarray_out must be in the incoming parameter list above.

            for ( var crec = 0; crec < recarray.length; crec++ )
		{
                    var rvgets;  /* String */
                    rvgets = recarray[crec].toString();

                    var buf   = new buf_t(rvgets.length+1);  // we'll tack on a \0
                    for ( var i = 0; i < rvgets.length; i++ )
			{  buf[i] = rvgets.charCodeAt(i);  // not sure if really necessary
			}
                    buf[rvgets.length] = 0;  // Hopefully, interpreted in C as \0
                    if ( conout == true )
			{ console.log( "Got input %s%s%s", "<<", rvgets, ">>" ); }

                    estat = gdp_gcl_publish_buf_js( gcl_Ptr, datum, buf );
                    // TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

                    // grab record number and timestamp for this newly written record
                    var ts = gdp_datum_getts_as_string_js( datum, true /* format */ );
                    // TBD: below check for 64-bit integer return type, gdp_recno_t
                    var rn = gdp_datum_getrecno_js( datum );
                    recarray_out[crec] = { recno: rn, time_stamp: ts };

                    if ( conout == true )
			{	gdp_datum_print_stdout_js( datum ); }
		} /* end for ( var crec = 0 ...) */
	}

    gdp_datum_free_js( datum );

    estat = gdp_gcl_close_js( gcl_Ptr );

    // TBD: fix this error return - see corresponding location in writer-test.js
    // string.repeat not available for us here in ECMASscript<6
    var str = new Array( 200 + 1 ).join( " " );  // long enough??
    var emsg = ( "exiting with status " +
            ep_stat_tostr_js(estat, str, str.length) );
    if ( conout == true )
	{	fflush_all_js();  // sometimes Node.js may not empty buffers
	    console.error( emsg );
	}
    // console.error( "exiting with status %s",
    // 				ep_stat_tostr_js(estat, str, str.length) );
    // OLD return ( ! ep_stat_isok_js(estat) );
    rv = {  error_isok: ( (ep_stat_isok_js(estat) == 0) ? false : true ),
            error_code: ( "0x" + estat.toString(16) ),
            error_msg:  emsg,
            gcl_name:   gcl_name
    };
    return rv;

} /* end function write_gcl_records() */



// ========================================================================
// Example set ups for calls to read_gcl_records()
// TBD1 Bring this doc up to date
// TBD update these examples and those in reader-test.js
// TBD recdest has been replaced by conout ??
//
// A: read gcl and write to stdout
// recdest = -1;  // writes the gcl records to stdout with
//                // readable formatting
//
// B: read the gcl into a JS Array and return the Array
// recdest =  0;  // read the gcl records into the Array {... records: }
//               TBD: Not Yet Implemented
//
// Finally, call the function:
// Returns:
// { error_code: EP_STAT, records: Array of Strings }
// recdest = -1;  // writes the gcl records to stdout...
// read_gcl_records( gdpd_addr, gcl_name,
//                   gcl_firstrec, gcl_numrecs,
//                   gcl_subscribe, gcl_multiread, recdest
//                 );
//
// Note, read_gcl_records() calls:
//   do_multiread(gcl_Ptr, gcl_firstrec, gcl_numrecs, gcl_subscribe, ... );
//   do_simpleread(gcl_Ptr, gcl_firstrec, gcl_numrecs, ... );

/* Returns:
   { error_isok: false|true, error_code: EP_STAT, error_msg: String,
   records: Array of records, each element with record data
   }
   where an element of the Array is:
   {
   recno:     <integer record number>,
   timestamp: <String timestamp of record>,
   value:     <String contents of record>
   }
*/
function read_gcl_records( gdpd_addr, gcl_name,
        gcl_firstrec, gcl_numrecs,
        gcl_subscribe, gcl_multiread, recdest,
        conout, gdp_event_cbfunc,
        /* Boolean */ wait_for_events
                           )

// gdpd_addr     gdp daemon's <host:port>; if null, use default "127.0.0.1:2468"
// gcl_name      name of existing GCL 
// gcl_firstrec, gcl_numrecs, gcl_subscribe, gcl_multiread
//               as for reader-test.js -f, -n, -s and -m cmd line options
// TBD recdest is not used anymore
// recdest = -1  writes the gcl records to stdout with readable formatting
// recdest =  0  read the gcl records into the return value's Array { records: }
// conout        Boolean
// Iff recdest == 0 and conout == true; the Array entries written to the gcl
// will also be echoed to console.log().  The other recdest destinations will
// ALL result in console.log() output; conout is ignored.
// Note, there still may be undesired output via console.log() and
// console.error(). TBD
{
    // Local working variables
    var gcl_Ptr; // gclh
    var estat;   // EP_STAT
    var gclname  = new gcl_name_t(32);
    var gclpname = new gcl_pname_t(GDP_GCL_PNAME_LEN);
    var recarray_out = [];  // will hold contents of records read


    estat = gdp_init_js( /* String */ gdpd_addr );
    // TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

    if ( conout == true )
	{	// allow thread to settle to avoid interspersed debug output
		// TBD check this
            sleep.sleep(1); // needed only for stdout and stderr
	}

    estat = gdp_parse_name_js( gcl_name, gclname );
    // TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

    var rv_str = gdp_printable_name_js( gclname, gclpname );
    if ( conout == true )
	{ console.log( "Reading GCL %s", array_to_String(gclpname) ); }

    // TBD: is this ref.alloc() necessary?
    var gclPtrPtr = ref.alloc( gdp_gcl_tPtrPtr );
    // TBD: check signature
    var gclopenrv = gdp_gcl_open_js(gclname, GDP_MODE_RO, gclPtrPtr);
    estat   = gclopenrv.error_code;
    gcl_Ptr = gclopenrv.gclH;
    // TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

    if (gcl_subscribe || gcl_multiread)
	{
            // DEBUG TBD1
            // console.log( 'In read_gcl_records(): before do_multiread()' );
            // true for reader-test.js; false for gdpREST_server.js
            estat = do_multiread( gcl_Ptr, gcl_firstrec, gcl_numrecs, gcl_subscribe,
                    wait_for_events,
                    recarray_out, conout, gdp_event_cbfunc
                                  );
	}
    else
	{
            estat = do_simpleread( gcl_Ptr, gcl_firstrec, gcl_numrecs,
                    recarray_out, conout
                                   );
	}
    // TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

    gdp_gcl_close_js( gcl_Ptr );

    // TBD: fix this error return - see corresponding location in reader-test.js
    // string.repeat not available for us here in ECMASscript<6
    var str = new Array( 200 + 1 ).join( " " );  // long enough??
    var emsg = ( "exiting with status " +
            ep_stat_tostr_js(estat, str, str.length) );
    if ( conout == true )
	{	fflush_all_js();  // sometimes Node.js may not empty buffers
	    console.error( emsg );
	}
    // console.error( "exiting with status %s",
    // 				ep_stat_tostr_js(estat, str, str.length) );
    // OLD return ( ! ep_stat_isok_js(estat) );
    rv = {  err:
            {
                error_isok: ( (ep_stat_isok_js(estat) == 0) ? false : true ),
                error_code: ( "0x" + estat.toString(16) ),
                error_msg:  emsg,
            },
            records:    recarray_out
    };
    return rv;
} /* end read_gcl_records( ) */


// ========================================================================


//C  /*
//C  **  DO_SIMPLEREAD --- read from a GCL using the one-record-at-a-time call
//C  */
//C  
//C  EP_STAT
//C  do_simpleread(gdp_gcl_t *gclh, gdp_recno_t firstrec, int numrecs)
//C  {
// Note, firstrec is a JS Number not a ref gdp_recno_t and, similarly,
//       numrecs is a JS Number not a ref int
/* EP_STAT */
function do_simpleread( gclh, firstrec, numrecs,
        recarray_out, conout
                        )
{
    //C  	EP_STAT estat = EP_STAT_OK;
    // ?? make sure this can hold & allow access to gdp EP_STAT's
    var estat = /* EP_STAT */ ep_stat_ok_js();
    //C  	gdp_recno_t recno;
    // but is recno's type correct for gdp_gcl_read() below?
    // Don't seem to work:
    //var recno = ref.alloc( gdp_recno_t, firstrec ); //?? check this
    //var recno = ref.alloc( gdp_recno_t, 0 ); //?? check this
    // OLD var recno = firstrec;
    // Just settle for a JS var and hope for the best - seems to work
    var recno;
    //C  	gdp_datum_t *datum = gdp_datum_new();
    var datum;
    datum = gdp_datum_new_js();
    // TBD where is datum freed?
    //C  
    //C  	// change the "infinity" sentinel to make the loop easier
    if ( numrecs == 0 ) numrecs = -1;
    //C  
    //C  	// can't start reading before first record (but negative makes sense)
    if ( firstrec == 0 ) firstrec = 1;
    //C  
    //C  	// start reading data, one record at a time
    recno = firstrec;
    var crec = 0;  // counts records read - an index into recarray_out[]
    while (numrecs < 0 || --numrecs >= 0)
        {
            //C  		// ask the GDP to give us a record
            estat = gdp_gcl_read_js(gclh, recno, datum);
            //C  
            //C  		// make sure it did; if not, break out of the loop
            if ( ! ep_stat_isok_js(estat) ) { break; }
            //C  
            if ( conout == true )
                {
                    //C  		// print out the value returned
                    //C  		fprintf(stdout, " >>> ");
                    // Note, when writing to a file rather that a terminal writes using
                    // process.stdout.write() will not be synchronized with writes 
                    // using console.log()
                    process.stdout.write( " >>> " );
                    // Tried an explicit fflush() here but Node.js prints the ' >>> '
                    // and then crashes, a la:  ' >>> Bus error: 10'
                    //   libc.fflush();
                    // For some reason this call to libgdpjs.fflush_all() works:
                    fflush_all_js();  // sometimes Node.js may not empty buffers
                    //C  		gdp_datum_print(datum, stdout);
                    gdp_datum_print_stdout_js( datum );
                }
            //C  
            // TBD1 - error checks
            // grab record contents for this newly read record
            var val = get_datum_buf_as_string( datum );
            // grab record number and timestamp for this newly read record
            var ts = gdp_datum_getts_as_string_js( datum, true /* format */ );
            // TBD: below check for 64-bit integer return type, gdp_recno_t
            var rn = gdp_datum_getrecno_js( datum );
            // TBD: check that recno and rn agree - which to use here?
            recarray_out[crec] = 
	        {
                    recno:     rn,   // for now we use gdp's record number
                    timestamp: ts,
                    value:     val
	        };
            crec++;
            //C  		// move to the next record
            recno++;
            //C  
            // For now, we live dangerously & leave any flushing to the next guy
            // TBD??
            //C  		// flush any left over data
            //C  		if (gdp_buf_reset(gdp_datum_getbuf(datum)) < 0)
            //C  		{
            //C  			char nbuf[40];
            //C  
            //C  			strerror_r(errno, nbuf, sizeof nbuf);
            //C  			printf("*** WARNING: buffer reset failed: %s\n",
            //C  					nbuf);
            //C  		}
        } /* while */
    //C  
    //C  	// end of data is returned as a "not found" error: turn it into a warning
    //C  	//    to avoid scaring the unsuspecting user
    if ( ep_stat_is_same_js( estat, gdp_stat_nak_notfound_js() ) )
        {
            estat = ep_stat_end_of_file_js();
        }
    return estat;
} /* end do_simpleread() */
//C  }
//C  
// ========================================================================
//C  
//C  /*
//C  **  DO_MULTIREAD --- subscribe or multiread
//C  **
//C  **		This routine handles calls that return multiple values via the
//C  **		event interface.  They might include subscriptions.
//C  */
//   TBD: document the additional functionality we have added to the original
//   C version of this function:
//	      wait_for_events, recarray_out, conout, gdp_event_cbfunc
//   Note: gdp_event_cbfunc is, currently, not all that useful since it's
//   not really Node.js asynchrouous -- there is no handling of C-level
//   callbacks (which, BTW, aren't yet implemented).
//   TBD ensure wait_for_events == true/false works for gdp_gcl_multiread..()
//   TBD to better handle errors, put error info in a structured return value
//C  
//C  EP_STAT
//C  do_multiread(gdp_gcl_t *gclh, gdp_recno_t firstrec, int32_t numrecs, bool subscribe)
//C  {
// Note, firstrec is a JS Number not a ref gdp_recno_t and, similarly,
//      true numrecs is a JS Number not a ref int32_t
/* EP_STAT */
function do_multiread( gclh, firstrec, numrecs, subscribe, 
        /* Boolean */ wait_for_events,
        recarray_out, conout, gdp_event_cbfunc
                       )
{
    //C  	EP_STAT estat;
    var estat;  //?? make sure this can hold & allow access to gdp EP_STAT's
    //C  
    // DEBUG
    // console.log( 'In rw_supt.js: do_multiread() At A' );
    if (subscribe)
        {
            //C  		// start up a subscription
            estat = gdp_gcl_subscribe_no_timeout_no_callback_js( 
                    gclh, firstrec, numrecs );
        }
    else
        {
            //C  		// make the flags more user-friendly
            if ( firstrec == 0 ) firstrec = 1;
            //C  
            //C  		// start up a multiread
            //C  		estat = gdp_gcl_multiread(gclh, firstrec, numrecs, NULL, NULL);
            // TBD1 - does this case still work for testing and for server use?
            estat = gdp_gcl_multiread_no_callback_js( gclh, firstrec, numrecs );
        }
    //C  
    //C  	// check to make sure the subscribe/multiread succeeded; if not, bail
    // DEBUG
    // console.log( 'In rw_supt.js: do_multiread() At B' );
    if ( ! ep_stat_isok_js(estat) )
        {
            //C  		char ebuf[200];
            //C  
            //C  		ep_app_abort("Cannot %s:\n\t%s",
            //C  				subscribe ? "subscribe" : "multiread",
            //C  				ep_stat_tostr(estat, ebuf, sizeof ebuf));
            // No need for ep_app_abort's PRINTFLIKE behavior here - just
            // give an error msg and exit.
            // ??consider var ebuf = new buf_t(100);
            // TBD place this error info in a structured return value
            var ebuf = new Array( 200 + 1 ).join( " " );  // long enough??
            console.error( "Cannot %s:\n\t%s",
                    subscribe ? "subscribe" : "multiread",
                    ep_stat_tostr_js(estat, ebuf, ebuf.length)
                           );
            fflush_all_js();  // sometimes Node.js may not empty buffers
            // TBD! not acceptable error behavior for a server!
            process.exit(1);  // could have better error code; e.g., EX_USAGE
        }
    //C  
    // DEBUG
    // console.log( 'In rw_supt.js: do_multiread() At C' );
    var crec = 0;  // counts records read - an index into recarray_out[]
    //C  	// now start reading the events that will be generated
    for (;;)
        {
            //C  		// get the next incoming event
            //C         gdp_event_t *gev = gdp_event_next(true);
            var gev_Ptr; // gev
            var evtype_int;
            // OLD gev_Ptr = gdp_event_next_js(true);
            // if wait_for_events == false, do a single
            // synchronous poll here for a currently waiting gdp event; don't 
            // wait for any future events; if there is no currently waiting
            // event, return immediately with a null
            // if wait_for_events == true, wait (indefinitely)
            // for a next gdp event
            gev_Ptr = gdp_event_next_js( wait_for_events );
            // TBD: what if gev_Ptr is null; happens on gdp_event_next_js(false)
            //      with no event available.
            // DEBUG
            // console.log( 'In rw_supt.js: do_multiread() At D; gev_Ptr = ', gev_Ptr );
            // if ( gev_Ptr == null )
            if ( gev_Ptr.isNull() )
                {	// no next event found - just return with no side-effects
                    // on recarray_out, or call of gdp_event_cbfunc.
                    // DEBUG
                    // console.log( 'In rw_supt.js: do_multiread() At E' );
                    return estat;
                }
            else
                {	// we have seen an event - process it based on its type
			// DEBUG
			// console.log( 'In rw_supt.js: do_multiread() At F' );
                    evtype_int = gdp_event_gettype_js(gev_Ptr);
                    //C  
                    //C  		// decode it
  		    switch ( evtype_int )
                        {
                        case GDP_EVENT_DATA:
                            //C  			// this event contains a data return
			    var datum = gdp_event_getdatum_js( gev_Ptr );
                            if ( conout == true )
				{
                                    //C  			fprintf(stdout, " >>> ");A
                                    // See process.stdout.write comments above in do_simpleread() .
                                    process.stdout.write( " >>> " );
                                    fflush_all_js();  // sometimes Node.js may not empty buffers
                                    gdp_datum_print_stdout_js( datum );
				}
  			    if ( gdp_event_cbfunc )
                                {
                                    // TBD1 - cleanup here
                                    // do this stashing into recarray_out even if we
                                    // don't have a non-null gdp_event_cbfunc()
                                    // grab record contents for this newly read record
                                    var val = get_datum_buf_as_string( datum );
                                    // grab record number and timestamp for this newly read record
                                    var ts = gdp_datum_getts_as_string_js( datum, true /* format */ );
                                    // TBD: below check for 64-bit integer return type, gdp_recno_t
                                    var rn = gdp_datum_getrecno_js( datum );
                                    // TBD: check that recno and rn agree - which to use here?
                                    recarray_out[crec] = 
					{
                                            recno:     rn,   // for now we use gdp's record number
                                            timestamp: ts,
                                            value:     val
					};
                                    crec++;
                                    // DEBUG
                                    // console.log( 'In rw_supt.js: do_multiread() At G' );
                                    // TBD1 - cleanup here
                                    gdp_event_cbfunc( evtype_int, datum, recarray_out );
				}

                            break;
                            //C  
                        case GDP_EVENT_EOS:
                            //C  			// "end of subscription": no more data will be returned
                            //C  			fprintf(stdout, "End of %s\n",
                            //C  					subscribe ? "Subscription" : "Multiread");
                            // TBD should we also return this as error info?
                            if ( conout == true )
				{ console.log("End of %s", subscribe ? "Subscription" : "Multiread" ); }
  			    if ( gdp_event_cbfunc )
                                { 
                                    // DEBUG
                                    // console.log( 'In rw_supt.js: do_multiread() At G' );
                                    gdp_event_cbfunc( evtype_int, null, estat );
				
				}
  			    return estat;
                            //C  
                        default:
                            //C  			// should be ignored, but we print it since this is a test program
                            //C  			fprintf(stderr, "Unknown event type %d\n", gdp_event_gettype(gev));
                            // TBD handle this error situation more usefully
                            // TBD should we also return this as error info?
                            if ( conout == true )
				{
                                    console.error( "Unknown event type %d\n",
                                            gdp_event_gettype_js(gev_Ptr) );
                                    fflush_all_js();  // sometimes Node.js may not empty buffers
				}
                            //C  
                            //C  			// just in case we get into some crazy loop.....
                            // TBD check the need for this in server code
			    sleep.sleep(1);
  			    break;
                        } /* switch */
                    //C  
                    //C  		// don't forget to free the event!
                    // protect against freeing nulls
                    if ( gev_Ptr != null )
			{ gdp_event_free_js(gev_Ptr); }

                } /* end if ( gev_Ptr == null ) else */
        } /* for (;;) */
    //C  	
    //C  	// should never get here
    return estat;
} /* end do_multiread() */
//C  }
//C  

