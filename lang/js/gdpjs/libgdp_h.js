/* vim: set ai sw=4 sts=4 ts=4 : */

// libgdp_h.js - GDP Javascript "header file"
// 2014-11-02
// Alec Dara-Abrams

// TBD: put this code into a Node.js module; add Copyrights

// Provides Node.js Javascript definitions for use in accessing the GDP
// from a Node.js Javascript program.
//
// In particular these definitions are used in:
//     writer-test.js - a JS version of gdp/apps/writer-test.c
//     reader-test.js - "               gdp/apps/reader-test.c

// Important build and execute parameters:
// They MUST be supplied by the JS program that "includes" this file.
// The values supplied as defaults below are for a including JS program
// which resides in the same directory as this file.
// 
//
// Path to the gdp/ directory.  Importantly, with a libs/ subdirectory
// holding the gdp dynamic libraries:
//    gdp/libs/libep,  gdp/libs/libgdp
// Be aware of particular major and minor version numbers in these names.
// See uses of ffi.Library( GDP_DIR +  ) below.
// Note: Here we are using a relative path for our default value:
if (GDP_DIR == undefined) GDP_DIR = process.env.GDP_DIR;
if (GDP_DIR == undefined) GDP_DIR = "../../..";

// Path to the gdp/lang/js/gdpjs/ directory where this file usually resides.
// And which also has a libs/ subdirectory holding the gdpjs dynamic
// library:  gdpjs/../libs/libgdpjs.0.1
// See uses of ffi.Library( GDPJS_DIR +  ) below.
// Note: Here we are using a relative path for our default value:
// ?? OLD if ( GDPJS_DIR == undefined) GDPJS_DIR  = "./";
if (GDPJS_DIR == undefined) GDPJS_DIR = GDP_DIR + "/lang/js/gdpjs";

// Path to a directory containing the proper node_modules/ directory.
// See the require()'s below.
// Note: a null path prefix, evidently, means use the usual node module
// lookup mechanism (below); otherwise if the prefix is not null, ONLY
// look for the module in the directory:  prefix + <<module name>> .
if (NODE_MODULES_DIR == undefined) NODE_MODULES_DIR = "";


// To effectively "include" the text of this file in a JS program file
// that needs the definitions here, consider the device:
//
//   // Parameters needed for libgdp_h.js
//   // They MUST be adapted to the directory where this program will be run.
//   // See libgdp_h.js for details.
//   var GDP_DIR           = "../../../";
//   var GDPJS_DIR         = GDP_DIR + "./lang/js/gdpjs/";
//   var NODE_MODULES_DIR  = "";
//   //
//   var LIBGDP_H_DIR = GDPJS_DIR;
//   // Here we include and evaluate our shared GDP Javascript "header file",
//   // 'libgdp_h.js', within the global scope/environment.
//   var fs = require('fs');   // Node.js's built-in File System module
//   eval( fs.readFileSync( LIBGDP_H_DIR + 'libgdp_h.js').toString() );
//
// Note, we can't usefully wrap the include device within a JS function.  
// In such a case, new names would only be available within the scope of
// that function.
//
// We give due credit for this near-kludge, JS "include" device:
//    http://stackoverflow.com/questions/5797852/in-node-js-how-do-i-include-functions-from-my-other-files


// Provenance:
//
// This file is derived (by hand) from the gdp/gdp/ and gdp/ep/ include files,
// from writer-test.c and reader-test.c, as well as from some system include
// files.  See the comments below for details.
//
// Lines of C code (including all their comments & blank likes) from
// writer-test.c or reader-test.c are prefixed with "//C  ".
//
// Lines of C code imported from other GDP .[ch] files and system .h files
// are included here as JS comments and are prefixed with "//CJS".
//
// Regular (local) JS comments are prefixed with "//".
//
// If you find the included C code comments annoying, consider removing these
// lines with something like:
//    sed  -e '/^\/\/C/d' liggdp_h.js


// Load Node.js modules for calling foreign functions; C functions here.
// The node modules below are installed locally in this file's directory
// via npm.
// It's not clear yet what path(s) node (the Node.js executable) uses to
// locate modules.  By default, it seems that node only looks for node_modules
// directories up the path from the current directory; it does not by default
// look in the "global" node_modules directory (which defaults to
// /usr/local/lib/node_modules/ ).
//
var ffi = require(NODE_MODULES_DIR + 'ffi');
var ref = require(NODE_MODULES_DIR + 'ref');
var ref_array = require(NODE_MODULES_DIR + 'ref-array');
var ref_struct = require(NODE_MODULES_DIR + 'ref-struct');
// var ref_union  = require( 'ref-union'  );   // currently, not used

// Some additional supporting Node.js modules
//
// Node.js's built-in utilities for file paths
var path = require('path');
//
// POSIX style getopt()
var mod_getopt = require(NODE_MODULES_DIR + 'posix-getopt');
//
// escape non-printing charcters
var jsesc = require(NODE_MODULES_DIR + 'jsesc');
//
// for a *NIX-like sleep(3), (synchronous - so not be very Node.js-ish)
var sleep = require(NODE_MODULES_DIR + 'sleep');
//
// The npm module 'printf', useful for debugging, seems to have problems.
// A use like: printf( "estat = %16X\n", estat ); may cause a core dump??
// var printf = require( 'printf' );




var gdp_gcl_open_info_tPtr = ref.refType(ref.types.void);

// { GRRRRRR... trying to get stdout up to the JS level using libc.fcntl()
// here JUST DOESN'T WORK.  Get a Segmentation fault: 11 on call to
// libgdp.gdp_datum_print().
// We bail out to adding libgdpjs.gdp_datum_print_stdout() below.
// This cost three hours+.

// From <unistd.h>
const STDIN_FILENO = 0; /* standard input file descriptor */
const STDOUT_FILENO = 1; /* standard output file descriptor */
const STDERR_FILENO = 2; /* standard error file descriptor */

// From <fcntl.h> ==> <sys/fcntl.h>
//CJS /* open-only flags */
const O_RDONLY = 0x0000; /* open for reading only */
const O_WRONLY = 0x0001; /* open for writing only */
const O_RDWR = 0x0002; /* open for reading and writing */
const O_ACCMODE = 0x0003; /* mask for above modes */

//CJS /* fcntl(2) command values */
const F_DUPFD = 0 /* duplicate file descriptor */
    // GRRRRRR }

// From libc - for local use here
var libc = ffi.Library('libc', {

    // From <sys/fcntl.h> and see man pages open(2) and fcntl(2)
    // But see note on fflush() below.  Makes these node-ffi functions suspect.

    //CJS int     open(const char *, int, ...) __DARWIN_ALIAS_C(open);
    // Use: fp_stdout_open  = libc.open( "/dev/stdout", /* mode = */ O_WRONLY );
    'open': ['pointer', ['string', 'int']], //?? correct?

    //CJS int     creat(const char *, mode_t) __DARWIN_ALIAS_C(creat);
    'creat': ['pointer', ['string', 'int']], //?? correct?

    //CJS int     fcntl(int, int, ...) __DARWIN_ALIAS_C(fcntl); 
    // Use: fp_stdout_fcntl = libc.fcntl( STDOUT_FILENO, F_DUPFD, 0 );
    'fcntl': ['pointer', ['int', 'int', 'int']], //?? correct?

    // From <stdio.h>  -- see man page fgets/gets(3), of course
    //CJS char    *gets(char *);
    'gets': ['string', ['string']], //?? seems to work

    // <stdio.h>  -- see man page fflush(3)
    // Though, an actual call to libc.fflush() seems to crash Node.js
    // See the note at the first print of " >>> " below.
    // But, libgdpjs.fflush_all() seems to work (no Node.js crash).
    'fflush': ['int', []]

});


// First, some definitions culled from gdp/gdp.h and system includes
// that are referenced in ep/ep_xxxx.h and gdp/gdp_xxxx.h .

// From ?? ==> <sysexits.h>
//CJS #define EX_USAGE        64      /* command line usage error */
var EX_USAGE = 64;

// From gdp/gdp.h => <inttypes.h> ==> <stdint.h> ==> <sys/_types/_int32_t.h>
var int32_t = ref.types.int32;

// From gdp/gdp.h => <stdbool.h> 
var bool_t = ref.types.int;


// Now we provide some definitions and functions from libep

// First, some C types to define for Node.js ffi/ref modules.
// From ep/ep_stat.h
//CJS typedef struct _ep_stat { uint32_t code; } EP_STAT;
var EP_STAT = 'uint32';

// From ep/ep_time.h
//CJS 
//CJS typedef struct
//CJS {
//CJS 	int64_t		tv_sec;		// seconds since Jan 1, 1970
//CJS 	uint32_t	tv_nsec;	// nanoseconds
//CJS 	float		tv_accuracy;	// clock accuracy in seconds
//CJS } EP_TIME_SPEC;
//CJS 
// Field types - and, hence, sizes
var tv_sec_t = 'int64'; //?? check this size
var tv_nsec_t = 'uint32';
var tv_accuracy_t = 'float';
//
// Define the EP_TIME_SPEC_struct JS "struct" type
// Note, since ep/ep_tine.h doesn't define a typedef here, just struct
// EP_TIME_SPEC, we use the JS naming convention "<C struct name>_struct"
// rather that the "<C typedef name>_t" JS convention we use for 
// typedef struct gdp+gcl gcp_gcl_t below.  Sorry if this is pedantic, but
// we just don't have any compile-time type checking here on the JS side of
// our FFI interface - so we use naming convention as a weak crutch.
// Also note, unlike gdp_gcl_t, EP_TIME_SPEC_struct is NOT opaque for us
// up here in JS.
var EP_TIME_SPEC_struct = ref_struct({
    tv_sec: tv_sec_t,
    tv_nsec: tv_nsec_t,
    tv_accuracy: tv_accuracy_t
});
//
// a pointer to a C "struct EP_TIME_SPEC"
var EP_TIME_SPEC_struct_Ptr = ref.refType(EP_TIME_SPEC_struct);
//?? just below is not used yet
var EP_TIME_SPEC_struct_PtrPtr = ref.refType(EP_TIME_SPEC_struct_Ptr);


var libep = ffi.Library(GDP_DIR + '/libs/libep-3.0', {

    // From ep/ep_dbg.h
    //CJS // initialization
    //CJS extern void     ep_dbg_init(void);
    // May be called by gdp_init() .
    'ep_dbg_init': ['void', []],

    //CJS // setting debug flags
    //CJS extern void     ep_dbg_set(const char *s);
    'ep_dbg_set': ['void', ['string']],

    // From ep/ep_app.h
    //CJS extern const char       *ep_app_getprogname(void);
    'ep_app_getprogname': ['string', []],

    // From ep/ep_stat.h
    //CJS // return string representation of status
    //CJS char            *ep_stat_tostr(EP_STAT estat, char *buf, size_t bsize);
    // We only get a string back via the char* return value; buf seems unchanged
    'ep_stat_tostr': ['string', [EP_STAT, 'string', 'size_t']],

    // From ep/ep_time.h
    //CJS // return current time
    //CJS extern EP_STAT	ep_time_now(EP_TIME_SPEC *tv);
    // TBD add this calling information to other node-ffi functions
    // How to call from JS:
    //   var ts = new EP_TIME_SPEC_struct;
    //   estat  = libep.ep_time_now( ts.ref() );
    //   now_secs = ts.tv_sec; now_nsecs = ts.tv_nsec; now_acc = tspec.tv_accuracy;
    'ep_time_now': [EP_STAT, [EP_TIME_SPEC_struct_Ptr]],

    // From ep/ep_time.h
    //CJS // return putative clock accuracy
    //CJS extern float	ep_time_accuracy(void);
    'ep_time_accuracy': ['float', []],

    // From ep/ep_time.h
    //CJS // set the clock accuracy (may not be available)
    //CJS extern void	ep_time_setaccuracy(float acc);
    //  'ep_time_setaccuracy': [ 'void', [ 'float' ] ],

    // From ep/ep_time.h
    // There are problems accessing ep_time_format() using node-ffi with this sig.
    // Use ep_time_as_string_js() instead for access from JS -- via
    // gdpjs_supt.c/ep_time_as_string().  TBD
    //CJS // format a time string into a buffer
    //CJS extern void	ep_time_format(const EP_TIME_SPEC *tv,
    //CJS 				char *buf,
    //CJS 				size_t bz,
    //CJS 				bool human);
    // 'ep_time_format': [ 'void', 
    //                     [ EP_TIME_SPEC_struct_Ptr, 'string', 'size_t', bool_t ] ],

    // From ep/ep_time.h
    //CJS // parse a time string
    //CJS extern EP_STAT	ep_time_parse(const char *timestr,
    //CJS 				EP_TIME_SPEC *tv);
    'ep_time_parse': [EP_STAT, ['string', EP_TIME_SPEC_struct_Ptr]]

});


// From gdp/gdp.h
//CJS // the internal name of a GCL
//CJS typedef uint8_t gcl_name_t[32];
// YET TO DO: review all these node-ffi/ref "types" for portability
var uint8_t = ref.types.uint8;
var gcl_name_t = ref_array(uint8_t);
// DEBUG
// console.log( "gcl_name_t.size = ", gcl_name_t.size ); Outputs 8
// Below outputs: "function ArrayType(data, length) { .... many lines more }
// console.log( "var gcl_name_t = ref_array(uint8_t);", gcl_name_t )

// From gdp/gdp.h
//CJS // the printable name of a GCL
//CJS #define GDP_GCL_PNAME_LEN 43   // length of an encoded pname
var GDP_GCL_PNAME_LEN = 43;
//CJS typedef char gcl_pname_t[GDP_GCL_PNAME_LEN + 1];
var char_t = ref.types.char;
var gcl_pname_t = ref_array(char_t);

// From gdp/gdp.h
//CJS // a GCL record number
//CJS typedef int64_t                         gdp_recno_t;
var gdp_recno_t = ref.types.int64; //?? check this size

// From gdp/gdp.h
//CJS typedef enum
//CJS {
//CJS         GDP_MODE_ANY = 0,       // no mode specified
//CJS         GDP_MODE_RO = 1,        // read only
//CJS         GDP_MODE_AO = 2,        // append only
//CJS } gdp_iomode_t;
var GDP_MODE_ANY = 0;
var GDP_MODE_RO = 1;
var GDP_MODE_AO = 2;
var gdp_iomode_t = ref.types.int; //?? check this - enum === int ?

var char_t = ref.types.char;
var buf_t = ref_array(char_t);

// From gdp/gdp.h
//CJS  // an open handle on a GCL (opaque)
//CJS  typedef struct gdp_gcl gdp_gcl_t;
//
// So far, we don't need to look inside this struct or other GDP structs
// These comments are reminders of the internal structure and we hope can
// be removed in the future.
// From gdp/gdp_priv.h
// struct gdp_gcl
// {
//         EP_THR_MUTEX     mutex;
// //      EP_THR_COND      cond;       // Note, commented out in gdp_priv.h
//         struct req_head  reqs;
//         gcl_name_t       gcl_name;
//         gdp_iomode_t     iomode;
//         long             ver;
//         FILE             *fp;
//         void             *log_index;
//         off_t            data_offset;
// };
//
var gdp_gcl_t = ref.types.void; // opaque for us up here in JS
// a pointer to a C "typedef gdp_gcl_t"
var gdp_gcl_tPtr = ref.refType(gdp_gcl_t);
var gdp_gcl_tPtrPtr = ref.refType(gdp_gcl_tPtr); // pointer to a pointer

//CJS typedef struct gdp_datum        gdp_datum_t;

var gdp_datum_t = ref.types.void; // opaque for us up here in JS
var gdp_datum_tPtr = ref.refType(gdp_datum_t);
var gdp_datum_tPtrPtr = ref.refType(gdp_datum_tPtr); //?? not used yet

// From gdp/gdp_buf.h
//CJS typedef struct evbuffer gdp_buf_t;

var gdp_buf_t = ref.types.void; // opaque for us up here in JS
var gdp_buf_tPtr = ref.refType(gdp_buf_t);
var gdp_buf_tPtrPtr = ref.refType(gdp_buf_tPtr); //?? not used yet

// From gdp/gdp.h
//CJS typedef struct gdp_event        gdp_event_t;

var gdp_event_t = ref.types.void; // opaque for us up here in JS
var gdp_event_tPtr = ref.refType(gdp_event_t);
var gdp_event_tPtrPtr = ref.refType(gdp_event_tPtr);


// Again, just a reminder of the internal structure. Remove in the future.
// From gdp/gdp_event.h
// struct gdp_event
// {       
//         TAILQ_ENTRY(gdp_event)  queue;          // free/active queue link
//         int                     type;           // event type
//         gdp_gcl_t               *gcl;           // GCL handle for event
//         gdp_datum_t             *datum;         // datum for event
// };   

// From gdp/gdp.h
//CJS // event types
//CJS #define GDP_EVENT_DATA          1       // returned data
//CJS #define GDP_EVENT_EOS           2       // end of subscription
var GDP_EVENT_DATA = 1 // returned data
var GDP_EVENT_EOS = 2 // end of subscription


var libgdp = ffi.Library(GDP_DIR + '/libs/libgdp-0.5', {

    // From gdp/gdp.h
    //CJS // free an event (required after gdp_event_next)
    //CJS extern EP_STAT                  gdp_event_free(gdp_event_t *gev);
    'gdp_event_free': [EP_STAT, [gdp_event_tPtr]],

    //CJS // get next event (fills in gev structure)
    //CJS extern gdp_event_t              *gdp_event_next(bool wait);
    'gdp_event_next': [gdp_event_tPtr, [bool_t]],

    //CJS // get next event (fills in gev structure)
    //CJS extern gdp_event_t              *gdp_event_next(bool wait);

    //CJS // get the type of an event
    //CJS extern int                       gdp_event_gettype(gdp_event_t *gev);
    'gdp_event_gettype': ['int', [gdp_event_tPtr]],

    //CJS // get the GCL handle
    //CJS extern gdp_gcl_t                *gdp_event_getgcl(gdp_event_t *gev);
    'gdp_event_getgcl': [gdp_gcl_tPtr, [gdp_event_tPtr]],

    //CJS // get the datum
    //CJS extern gdp_datum_t              *gdp_event_getdatum(gdp_event_t *gev);
    'gdp_event_getdatum': [gdp_datum_tPtr, [gdp_event_tPtr]],

    // From gdp/gdp_event.h
    //CJS // allocate an event
    //CJS extern EP_STAT                  _gdp_event_new(gdp_event_t **gevp);
    // _gdp_event_new is no longer present.
    //   '_gdp_event_new': [ EP_STAT, [ gdp_event_tPtrPtr ] ],

    //CJS // add an event to the active queue
    //CJS extern void                      _gdp_event_trigger(gdp_event_t *gev);
    // _gdp_event_trigger is no longer present.
    //   '_gdp_event_trigger': [ 'void', [ gdp_event_tPtr ] ],

    // From gdp/gdp.h
    //CJS // initialize the library
    //CJS EP_STAT gdp_init( const char *gdpd_addr );          // address of gdpd
    'gdp_init': [EP_STAT, ['string']],

    // From gdp/gdp.h
    //CJS // create a new GCL
    //CJS EP_STAT gdp_gcl_create( gcl_name_t, gdp_gcl_t ** ); // pointer to result GCL handle
    'gdp_gcl_create': [EP_STAT, [gcl_name_t, gdp_gcl_tPtrPtr]],

    // From gdp/gdp.h
    //CJS // open an existing GCL
    //CJS extern EP_STAT  gdp_gcl_open( gcl_name_t name, gdp_iomode_t rw, gdp_gcl_t **gclh);
    'gdp_gcl_open': [EP_STAT, [gcl_name_t, gdp_iomode_t, gdp_gcl_open_info_tPtr, gdp_gcl_tPtrPtr]],

    // From gdp/gdp.h
    //CJS // close an open GCL
    //CJS EP_STAT  gdp_gcl_close( gdp_gcl_t *gclh);           // GCL handle to close
    'gdp_gcl_close': [EP_STAT, [gdp_gcl_tPtr]],

    'gdp_gcl_print': ['void', [gdp_gcl_tPtr, 'void']],

    // From gdp/gdp.h
    //CJS // make a printable GCL name from a binary version
    //CJS char *gdp_printable_name( const gcl_name_t internal, gcl_pname_t external);
    'gdp_printable_name': ['string', [gcl_name_t, gcl_pname_t]],

    // From gdp/gdp.h
    //CJS // parse a (possibly human-friendly) GCL name
    //CJS EP_STAT gdp_parse_name( const char *ext, gcl_name_t internal );
    'gdp_parse_name': [EP_STAT, ['string', gcl_name_t]],

    // From gdp/gdp.h
    //CJS // allocate a new message
    //CJS gdp_datum_t             *gdp_datum_new(void);
    'gdp_datum_new': [gdp_datum_tPtr, []],

    // From gdp/gdp.h
    //CJS // free a message
    //CJS void                    gdp_datum_free(gdp_datum_t *);
    'gdp_datum_free': ['void', [gdp_datum_tPtr]],

    // From gdp/gdp.h
    // print a message (for debugging)
    //CJS extern void gdp_datum_print( const gdp_datum_t *datum, FILE *fp );
    'gdp_datum_print': ['void', [gdp_datum_tPtr, 'pointer']],

    // From gdp/gdp.h
    //CJS // get the data buffer from a datum
    //CJS extern gdp_buf_t *gdp_datum_getbuf( const gdp_datum_t *datum );
    'gdp_datum_getbuf': [gdp_buf_tPtr, [gdp_datum_tPtr]],

    // From gdp/gdp_buf.h
    //CJS extern int gdp_buf_write( gdp_buf_t *buf, void *in, size_t sz );
    'gdp_buf_write': ['int', [gdp_buf_tPtr, buf_t, 'size_t']],

    // From gdp/gdp_buf.h
    //CJS extern size_t gdp_buf_read( gdp_buf_t *buf, void *out, size_t sz);
    'gdp_buf_read': ['size_t', [gdp_buf_tPtr, buf_t, 'size_t']],


    // From gdp/gdp.h
    //CJS // append to a writable GCL
    //CJS extern EP_STAT  gdp_gcl_append( gdp_gcl_t *gclh, gdp_datum_t *);
    'gdp_gcl_append': [EP_STAT, [gdp_gcl_tPtr, gdp_datum_tPtr]],

    // From gdp/gdp.h
    // subscribe to a readable GCL
    //CJS extern EP_STAT  gdp_gcl_subscribe(
    //CJS                   gdp_gcl_t *gclh,        // readable GCL handle
    //CJS                   gdp_recno_t start,      // first record to retrieve
    //CJS                   int32_t numrecs,        // number of records to retrieve
    //CJS                   EP_TIME_SPEC *timeout,  // timeout
    //CJS                   // callback function for next datum
    //CJS                   gdp_gcl_sub_cbfunc_t cbfunc,
    //CJS                   // argument passed to callback
    //CJS                   void *cbarg);
    // Note, in our call to this function in do_multiread() below we do not
    //       use the last three (pointer) arguments.
    'gdp_gcl_subscribe': [EP_STAT, [gdp_gcl_tPtr, gdp_recno_t, int32_t,
        'pointer', 'pointer', 'pointer'
    ]],

    // From gdp/gdp.h
    //CJS // read multiple records (no subscriptions)
    //CJS extern EP_STAT  gdp_gcl_multiread(
    //CJS                   gdp_gcl_t *gclh,        // readable GCL handle
    //CJS                   gdp_recno_t start,      // first record to retrieve
    //CJS                   int32_t numrecs,        // number of records to retrieve
    //CJS                   // callback function for next datum
    //CJS                   gdp_gcl_sub_cbfunc_t cbfunc,
    //CJS                   // argument passed to callback
    //CJS                   void *cbarg);
    // Note, in our call to this function in do_multiread() below we do not
    //       use the last two (pointer) arguments.
    'gdp_gcl_multiread': [EP_STAT, [gdp_gcl_tPtr, gdp_recno_t, int32_t,
        'pointer', 'pointer'
    ]],

    // From gdp/gdp.h
    //CJS // read from a readable GCL
    //CJS extern EP_STAT  gdp_gcl_read( gdp_gcl_t *gclh, gdp_recno_t recno, gdp_datum_t *datum);
    'gdp_gcl_read': [EP_STAT, [gdp_gcl_tPtr, gdp_recno_t, gdp_datum_tPtr]],


    // From gdp/gdp.h
    //CJS // get the record number from a datum
    //CJS extern gdp_recno_t      gdp_datum_getrecno(
    //CJS                                         const gdp_datum_t *datum);
    'gdp_datum_getrecno': [gdp_recno_t, [gdp_datum_tPtr]],

    // From gdp/gdp.h
    //CJS // get the timestamp from a datum
    //CJS extern void             gdp_datum_getts(
    //CJS                                         const gdp_datum_t *datum,
    //CJS                                         EP_TIME_SPEC *ts);
    // TBD: awaits our setup and testing of Node.js FFI access to structs
    // For now use ep_time_as_string()    -- in gdpjs_supt.c
    //         or  ep_time_as_string_js() -- in gdpjs_supt.js

    // From gdp/gdp.h
    //CJS // get the data length from a datum
    //CJS extern size_t   gdp_datum_getdlen( const gdp_datum_t *datum);
    'gdp_datum_getdlen': ['size_t', [gdp_datum_tPtr]],

    //   Below was used to test calling a C function with a very simple signature
    //   'log_view_ls': [ 'int', [ ] ]

});


// JS-to-GDP onion skin layer on selected GDP functions and macros.
//
var libgdpjs = ffi.Library(GDPJS_DIR + '../libs/libgdpjs-1.0', {

    // Some general libc-related functions

    // Forces a flush() on stdout, on stderr, and on all open file descriptors.
    // Node.js console.log(), console.error(), and process.stdxxx.write() are,
    // evidently, buffered when writing to a file.  Output to a terminal seems
    // to be unbuffered.
    'fflush_stdout': ['int', []],
    'fflush_stderr': ['int', []],
    'fflush_all': ['int', []],

    // Some general libgdp/libep-related functions

    // Returns the size in bytes of the libep error status code, EP_STAT .
    // Currently (2014-10-26) defined in ep/ep-stat.h, 
    //CJS	typedef struct _ep_stat
    //CJS	{   
    //CJS		uint32_t        code;
    //CJS	} EP_STAT;
    // This is a very specialized libgdp function.  I hope to use it to construct
    // node-ffi/ref types at JS execution time for some of the ep functions here.
    'sizeof_EP_STAT_in_bytes': ['size_t', []],

    // From gdp/gdp.h
    // print a GCL (for debugging)
    // Forwards to gcp_gcl_print( const gdp_gcl_t *gclh, stdout, , , );
    'gdp_gcl_print_stdout': ['void', [gdp_gcl_tPtr]],

    // From gdp/gdp.h
    // print a message (for debugging)
    // Forwards to gdp_datum_print( const gdp_datum_t *datum, stdout );
    'gdp_datum_print_stdout': ['void', [gdp_datum_tPtr]],

    // From gdp/gdp.h
    // Get a printable (base64-encoded) GCL name from an open GCL handle
    // Accesses gchlh->pname field directly
    'gdp_get_pname_from_gclh': ['string', [gdp_gcl_tPtr]],

    // From gdp/gdp.h
    // Get a printable (base64-encoded) GCL name from an open GCL handle
    // Uses gdp_gcl_getname() and gdp_printable_name()
    'gdp_get_printable_name_from_gclh': ['string', [gdp_gcl_tPtr]],

    // Get a timestamp as a string from an EP_TIME_SPEC
    // Note, we are returning a char* to a static variable; copy out its contents
    // quickly :-).  See gdpjs_supt.c for details.
    // Wraps ep/ep_time.h ep_time_format()
    'ep_time_as_string': ['string', [EP_TIME_SPEC_struct_Ptr, bool_t]],

    // Get a timestamp as a string from a datum
    // Note, we are returning a char* to a static variable; copy out its contents
    // quickly :-).  See gdpjs_supt.c for details.
    // Wraps gdp/gdp.h gdp_datum_getts() & ep/ep_time.h ep_time_format()
    'gdp_datum_getts_as_string': ['string', [gdp_datum_tPtr, bool_t]],

    // From gdp/gdp_stat.h
    //CJS #define GDP_STAT_NAK_NOTFOUND   GDP_STAT_NEW(ERROR, GDP_COAP_NOTFOUND)
    // 'gdp_stat_nak_notfound': [ 'int' /* (int) EP_STAT */, [ ] ],
    'gdp_stat_nak_notfound': [EP_STAT, []],

    // From ep/ep_statcodes.h
    //CJS // generic status codes
    //CJS #define EP_STAT_OK         EP_STAT_NEW(EP_STAT_SEV_OK, 0, 0, 0)
    'ep_stat_ok': [EP_STAT, []],

    // common shared errors
    // #define EP_STAT_END_OF_FILE   _EP_STAT_INTERNAL(WARN, EP_STAT_MOD_GENERIC, 3)
    'ep_stat_end_of_file': [EP_STAT, []],

    // From ep/ep_stat.h
    //CJS // predicates to query the status severity
    //CJS #define EP_STAT_ISOK(c)    (EP_STAT_SEVERITY(c) < EP_STAT_SEV_WARN)
    'ep_stat_isok': ['int' /* Boolean */ , [EP_STAT]],

    // From ep/ep_stat.h
    //CJS // compare two status codes for equality
    //CJS // #define EP_STAT_IS_SAME(a, b)   ((a).code == (b).code)
    'ep_stat_is_same': ['int' /* Boolean */ , [EP_STAT, EP_STAT]]
})
