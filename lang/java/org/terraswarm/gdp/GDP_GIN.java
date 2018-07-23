/* A Global Data Plane (GDP) GIN

   Copyright (c) 2015-2016 The Regents of the University of California.
   All rights reserved.
   Permission is hereby granted, without written agreement and without
   license or royalty fees, to use, copy, modify, and distribute this
   software and its documentation for any purpose, provided that the above
   copyright notice and the following two paragraphs appear in all copies
   of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY
   FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
   ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
   THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE
   PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE UNIVERSITY OF
   CALIFORNIA HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
   ENHANCEMENTS, OR MODIFICATIONS.

   PT_COPYRIGHT_VERSION_2
   COPYRIGHTENDKEY

 */

package org.terraswarm.gdp; 

import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.util.Date;
import java.util.Map;
import java.util.HashMap;
import java.awt.PointerInfo;
import java.lang.Exception;


import com.sun.jna.Native;
import com.sun.jna.Memory;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;
import org.terraswarm.gdp.NativeSize; // Fixed by cxh in makefile.

/**
 * A Global Data Plane (GDP) GIN
 *
 * <p>This Java wrapper may not have the latest features in the
 * underlying C implementation.</p>
 * 
 * @author Nitesh Mor, Christopher Brooks
 */
public class GDP_GIN {
    
    // The underscores in the names of public methods and variables should
    // be preserved so that they match the underlying C-based gdp code.

    /** 
     * Initialize a new Global Data Plane GIN
     *
     * Signatures are not yet supported, but are taken care of by the C library.
     *
     * @param name   Name of the log, which will be created if necessary.
     * @param iomode Should this be opened read only, read-append,
     * append-only.  See {@link #GDP_MODE}.
     * @param logdName  Name of the log server where this should be 
     *                  placed if it does not yet exist.
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp20Library.EP_STAT_SEV_WARN.
     */
    public GDP_GIN(GDP_NAME name, GDP_MODE iomode, GDP_NAME logdName) throws GDPException {
        // FIXME: No signatures support.
        System.out.println("GDP_GIN.java: GDP_GIN(" + name +
                           "(" + new String(name.printable_name()) +
                           "), " + iomode + ", " + logdName +
                           "(" + new String(logdName.printable_name()) +
                           ")");
        EP_STAT estat;
        
        PointerByReference ginhByReference = new PointerByReference();
        this.iomode = iomode;
        this.gclName = name.internal_name();
        
        // Open the GCL.
        GDP.dbg_set("*=20");
        estat = Gdp20Library.INSTANCE.gdp_gin_open(ByteBuffer.wrap(this.gclName), 
                            iomode.ordinal(), (PointerByReference) null, 
                            ginhByReference);
        if (!GDP.check_EP_STAT(estat)) {
            System.out.println("GDP_GIN: gdp_gin_open() failed, trying to create the log and call gdp_gin_open() again.");
            GDP_GIN.create(name, logdName);
            estat = Gdp20Library.INSTANCE.gdp_gin_open(ByteBuffer.wrap(this.gclName), 
                                                       iomode.ordinal(), (PointerByReference) null, 
                                                       ginhByReference);
        }
        GDP.check_EP_STAT(estat, "gdp_gin_open(" +
                          gclName + "(" + new String(name.printable_name(),0) + "), " +
                          iomode.ordinal() + ", null, " +
                          ginhByReference + ") failed.");

        // Associate the C pointer to this object.
        ginh = ginhByReference.getValue();
        assert this.ginh != null;

        // Add ourselves to the global map of pointers=>objects 
        _allGclhs.put(this.ginh, this);
    }

    ///////////////////////////////////////////////////////////////////
    ////                   public fields                           ////

    /**
     * The internal 256 bit name for the log.
     */
    public byte[] gclName;

    /**
     * A pointer to the C gdp_gin_t structure
     */
    public Pointer ginh = null;

    /**
     * The I/O mode for this log
     */
    public GDP_MODE iomode;

    /**
     * I/O mode for a log.
     * <ul>
     * <li> ANY: for internal use only </li>
     * <li> RO : Read only </li>
     * <li> AO : Append only </li>
     * <li> RA : Read + append </li>
     * </ul> 
     */
    public enum GDP_MODE {
        ANY, RO, AO, RA
    }

   
    ///////////////////////////////////////////////////////////////////
    ////                   public methods                          ////

    /**
     * Append data to a log. This will create a new record in the log.
     * 
     * @param datum_dict    A dictionary containing the key "data". The value
     *                      associated should be a byte[] containing the data
     *                      to be appended.
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp20Library.EP_STAT_SEV_WARN.
     */
    public void append(Map<String, Object>datum_dict) throws GDPException {

        EP_STAT estat;
        GDP_DATUM datum = new GDP_DATUM();
        
        Object data = datum_dict.get("data");
        datum.setbuf((byte[]) data);
        
        _checkGclh(ginh);
        estat = Gdp20Library.INSTANCE.gdp_gin_append(ginh,
                                datum.gdp_datum_ptr, null); // Let the C lib calculate prevHash
        GDP.check_EP_STAT(estat, "gdp_gin_append(" + ginh + ", " + datum.gdp_datum_ptr + ") failed.");

        return;
    }

    /**
     * Append data to a log. This will create a new record in the log.
     * 
     * @param data  Data that should be appended
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp20Library.EP_STAT_SEV_WARN.
     */
    public void append(byte[] data)  throws GDPException {
        
        HashMap<String, Object> datum = new HashMap<String, Object>();
        datum.put("data", data);
        
        this.append(datum);
    }

    /**
     * Append data to a log, asynchronously. This will create a new record in the log.
     * 
     * @param datum_dict    A dictionary containing the key "data". The value
     *                      associated should be a byte[] containing the data
     *                      to be appended.
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp20Library.EP_STAT_SEV_WARN.
     */
    public void append_async(Map<String, Object>datum_dict) throws GDPException {

        EP_STAT estat;
        GDP_DATUM datum = new GDP_DATUM();
        
        Object data = datum_dict.get("data");
        datum.setbuf((byte[]) data);
        
        _checkGclh(ginh);
        PointerByReference datums = new PointerByReference(datum.gdp_datum_ptr.getValue());
        estat = Gdp20Library.INSTANCE.gdp_gin_append_async(ginh,
                                1, datums, null, null, null);
        GDP.check_EP_STAT(estat, "gdp_gin_append_async(" + ginh + ", " + datum.gdp_datum_ptr + "null, null) failed.");

        return;
    }

    /**
     * Append data to a log, asynchronously. This will create a new record in the log.
     * 
     * @param data  Data that should be appended
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp20Library.EP_STAT_SEV_WARN.
     */
    public void append_async(byte[] data) throws GDPException {
        
        HashMap<String, Object> datum = new HashMap<String, Object>();
        datum.put("data", data);
        
        this.append_async(datum);
    }

    /** Close the GCL.
     */
    public void close() {
        // If close() is called twice, then the C code aborts the process.
        // See https://gdp.cs.berkeley.edu/redmine/issues/83

        //  Added synchronization, see https://gdp.cs.berkeley.edu/redmine/issues/107
        synchronized (ginh) {
            if (ginh != null) {
                // Remove ourselves from the global list.
                _allGclhs.remove(ginh);
                
                // Free the associated gdp_gin_t.
                Gdp20Library.INSTANCE.gdp_gin_close(ginh);
                ginh = null;
            }
        }
    }

    /**
     * Create a GCL.
     * 
     * @param logName   Name of the log to be created
     * @param logdName  Name of the log server where this should be 
     *                  placed.
     * @param metadata  Metadata to be added to the log. 
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp20Library.EP_STAT_SEV_WARN.
     */
    public static void create(GDP_NAME logName, GDP_NAME logdName, 
                    Map<Integer, byte[]> metadata) throws GDPException {
        System.out.println("GDP_GIN.java: create(" + logName + ", " + ", " + logdName + ", " + metadata + ")");
        EP_STAT estat;
        
        // Get the 256-bit internal names for log and logd
        ByteBuffer logNameInternal = ByteBuffer.wrap(logName.internal_name());
        ByteBuffer logdNameInternal = ByteBuffer.wrap(logdName.internal_name());
        
        // Just create a throwaway pointer.
        Pointer tmpPtr = null;
        
        // Metadata processing.
        GDP_MD m = new GDP_MD();
        for (int k: metadata.keySet()) {
            m.add(k, metadata.get(k));
        }
        
        estat = Gdp20Library.INSTANCE.gdp_gin_create(logNameInternal, logdNameInternal,
                        m.gdp_md_ptr, new PointerByReference(tmpPtr));
        
        GDP.check_EP_STAT(estat, "gdp_gin_create(" + 
                          logNameInternal + ", " +
                          logdNameInternal + ", " +
                          m.gdp_md_ptr + " new PointerByReference(" + tmpPtr + ")) failed.");
        
        return;
    }
    
    /**
     * Create a GCL (with empty metadata).
     * @param logName   Name of the log
     * @param logdName  Name of the logserver that should host this log
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp20Library.EP_STAT_SEV_WARN.
     */
    public static void create(String logName, String logdName) throws GDPException {
        GDP_GIN.create(new GDP_NAME(logName), new GDP_NAME(logdName));
    }
    /**
     * Create a GCL (with empty metadata).
     * @param logName   Name of the log
     * @param logdName  Name of the logserver that should host this log
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp20Library.EP_STAT_SEV_WARN.
     */
    public static void create(GDP_NAME logName, GDP_NAME logdName) throws GDPException {
        GDP_GIN.create(logName, logdName, new HashMap<Integer, byte[]>());
    }

    /** Remove the gcl from the global list and 
     *  free the associated pointer.
     *  Note that finalize() is only called when the 
     *  garbage collector decides that there are no
     *  references to an object.  There is no guarantee 
     *  that an object will be gc'd and that finalize
     *  will be called.
     */
     public void finalize() {
        close();
    }

    /** 
     * Create a new GCL.
     * This method is a convenience method used to create a GCL
     * when the GDP_MODE enum is not available, such as when
     * creating a GCL from JavaScript.
     *
     * @param name   Name of the log.
     * @param iomode Opening mode (0: for internal use only, 1: read-only, 2: read-append, 3: append-only)
     * @param logdName  Name of the log server where this should be 
     *                  placed if it does not yet exist.
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp20Library.EP_STAT_SEV_WARN.
     */
    public static GDP_GIN newGCL(GDP_NAME name, int iomode, GDP_NAME logdName) throws GDPException {
        System.out.println("GDP_GIN.java: newGCL(" + name + ", " + iomode + ", " + logdName + ")");
        // The GDP Accessor uses this method
        GDP_MODE gdp_mode = GDP_MODE.ANY;
        switch (iomode) {
        case 0:
            gdp_mode = GDP_MODE.ANY;
            break;
        case 1:
            gdp_mode = GDP_MODE.RO;
            break;
        case 2:
            gdp_mode = GDP_MODE.AO;
            break;
        case 3:
            gdp_mode = GDP_MODE.RA;
            break;
        default:
            throw new IllegalArgumentException("Mode must be 0-3, instead it was: " + iomode);
        }
        return new GDP_GIN(name, gdp_mode, logdName);
    }

    /**
     * Read a record by record number.
     * 
     * @param recno Record number to be read
     * @return A hashmap containing the data just read
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp20Library.EP_STAT_SEV_WARN.
     */
    public HashMap<String, Object> read_by_recno(long recno) throws GDPException {
        EP_STAT estat;
        GDP_DATUM datum = new GDP_DATUM();
                
        // Get the datum populated.
        _checkGclh(ginh);
        estat = Gdp20Library.INSTANCE.gdp_gin_read_by_recno(ginh, recno, 
                                datum.gdp_datum_ptr);
        GDP.check_EP_STAT(estat, "gdp_gin_read() failed.");

        HashMap<String, Object> datum_dict = new HashMap<String, Object>();
        if (datum != null) {
            try {
                datum_dict.put("recno", datum.getrecno());
                datum_dict.put("ts", datum.getts());
                datum_dict.put("data", datum.getbuf());
                // TODO: Add signature routines.
            } catch (Throwable throwable) {
                // In Nashorn, this exception sometimes is hidden, so
                // we print it here
                throwable.printStackTrace();
                throw throwable;
            }
        }
        return datum_dict;
    }

    /** 
     * Asynchronous version of read
     * only. Not for any future records
     * See the documentation in C library for examples.
     * 
     * @param firstrec  The record num to start reading from
     * @param numrecs   Max number of records to be returned. 
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp20Library.EP_STAT_SEV_WARN.
     */
    public void read_by_recno_async(long firstrec, int numrecs) throws GDPException {

        EP_STAT estat;
        _checkGclh(ginh);
        estat = Gdp20Library.INSTANCE
                    .gdp_gin_read_by_recno_async(ginh, firstrec, numrecs,
                                        null, null);

        GDP.check_EP_STAT(estat, "gdp_gin_read_by_recno_async() failed.");

        return;
    }

    /** 
     * Start a subscription to a log.
     * See the documentation in C library for examples.
     * 
     * @param firstrec  The record num to start the subscription from
     * @param numrecs   Max number of records to be returned. 0 => infinite
     * @param timeout   Timeout for this subscription
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp20Library.EP_STAT_SEV_WARN.
     */
    public void subscribe_by_recno(long firstrec, int numrecs,
                        EP_TIME_SPEC timeout) throws GDPException {

        EP_STAT estat;
        _checkGclh(ginh);
        estat = Gdp20Library.INSTANCE
                    .gdp_gin_subscribe_by_recno(ginh, firstrec, numrecs,
                                        timeout.getPointer(), null, null);

        GDP.check_EP_STAT(estat, "gdp_gin_subscribe_by_recno() failed.");
        return;
    }

    /** 
     * Get the next event.
     * 
     * @param timeout_msec  Time (in ms) for which to block. Can be used 
     *                      to block eternally as well.
     */
    public static HashMap<String, Object> get_next_event(
                        GDP_GIN obj, int timeout_msec) {
        // This method is used by the Ptolemy interface to the GDP.
        EP_TIME_SPEC timeout_spec = new EP_TIME_SPEC(timeout_msec/1000,
                0, /* nanoseconds */
                0.001f /* accuracy in seconds */);
        return get_next_event(obj, timeout_spec);
    }

    /** 
     * Get data from next record for a subscription or multiread
     * This is a wrapper around 'get_next_event', and works only 
     * for subscriptions, multireads.
     * 
     * @param timeout_msec  Time (in ms) for which to block. Can be used 
     *                      to block eternally as well.
     */
    public static HashMap<String, Object> get_next_event(
                        GDP_GIN obj, EP_TIME_SPEC timeout) {
        if (obj == null) {
            return _helper_get_next_event(null, timeout);
        } else {
            _checkGclh(obj.ginh);
            HashMap<String, Object> tmp = _helper_get_next_event(
                    obj.ginh, timeout);
            assert tmp.get("gcl_handle") == obj;

            return tmp;
        }
    }
    
    ///////////////////////////////////////////////////////////////////
    ////                   private methods                         ////

    /** 
     * Get data from next record for a subscription or multiread
     * This is a wrapper around 'get_next_event', and works only 
     * for subscriptions, multireads.
     * 
     * @param timeout_msec  Time (in ms) for which to block. Can be used 
     *                      to block eternally as well.
     */
    // public String get_next_data(int timeout_msec) {
    //     // This method is used by the Ptolemy interface to the GDP.
    //     EP_TIME_SPEC time_spec = new EP_TIME_SPEC(timeout_msec/1000,
    //             0, /* nanoseconds */
    //             0.001f /* accuracy in seconds */);

    //     // Returns next data item from subcription or multiread.
    //     // Returns null if the subscription has ended.
    //     GDP_EVENT ev = get_next_event(timeout_msec, time_spec);;
    //     if (ev.type == Gdp20Library.INSTANCE.GDP_EVENT_DATA) {
    //         return ev.datum.data;
    //     } else {
    //         return null;
    //     }

    // }
    
    public static HashMap<String, Object> _helper_get_next_event(
                            Pointer ginh, EP_TIME_SPEC timeout) {
        
        // Get the event pointer. ginh can be null.
        PointerByReference gdp_event_ptr = Gdp20Library.INSTANCE
                            .gdp_event_next(ginh, timeout);

        if (gdp_event_ptr == null) {
            return new HashMap<String, Object>();
            //throw new NullPointerException("gdp_event_next(" + ginh + ", " + timeout
            //        + ") returned null");
        }
        // Get the data associated with this event.
        // If gdp_event_gettype() is passed a NULL, then an assertion is thrown
        // and process exits.
        int type = Gdp20Library.INSTANCE.gdp_event_gettype(gdp_event_ptr);
        PointerByReference datum_ptr = Gdp20Library.INSTANCE
                            .gdp_event_getdatum(gdp_event_ptr);
        EP_STAT event_ep_stat = Gdp20Library.INSTANCE
                            .gdp_event_getstat(gdp_event_ptr);
        PointerByReference _ginhByReference = Gdp20Library.INSTANCE
                            .gdp_event_getgin(gdp_event_ptr);
        Pointer _ginh = _ginhByReference.getValue();
                            
        
        GDP_DATUM datum = new GDP_DATUM(datum_ptr);
        // create a datum dictionary.
        HashMap<String, Object> datum_dict = new HashMap<String, Object>();
        datum_dict.put("recno", datum.getrecno());
        datum_dict.put("ts", datum.getts());
        datum_dict.put("data", datum.getbuf());
        // TODO Fix signatures
        
        HashMap<String, Object> gdp_event = new HashMap<String, Object>();
        gdp_event.put("gcl_handle", _allGclhs.get(_ginh));
        gdp_event.put("datum", datum_dict);
        gdp_event.put("type", type);
        gdp_event.put("stat", event_ep_stat);
        
        // free the event structure
        Gdp20Library.INSTANCE.gdp_event_free(gdp_event_ptr);
        
        return gdp_event;
    }
    

    /**
     * Get next event as a result of subscription, multiread or
     * an asynchronous operation.
     * 
     * @param timeout_msec  Time (in ms) for which to block.
     */
    // public GDP_EVENT get_next_event(int timeout_msec) {
    //     // timeout in ms. Probably not very precise
    //     // 0 means block forever

    //     EP_TIME_SPEC timeout = null;

    //     if (timeout_msec != 0) {

    //         // Get current time
    //         Date d = new Date();
    //         long current_msec = d.getTime();

    //         // Get values to create a C timeout structure
    //         long tv_sec = (current_msec + timeout_msec)/1000;
    //         int tv_nsec = (int) ((current_msec + timeout_msec)%1000);
    //         float tv_accuracy = (float) 0.0;

    //         timeout = new EP_TIME_SPEC(tv_sec, tv_nsec, tv_accuracy);
    //     }

    //     // Get the C pointer to next event. This blocks till timeout
    //     PointerByReference gev = Gdp20Library.INSTANCE
    //                                 .gdp_event_next(this.ginh, timeout);

    //     // Get the data associated with this event
    //     int type = Gdp20Library.INSTANCE.gdp_event_gettype(gev);
    //     PointerByReference datum = Gdp20Library.INSTANCE.gdp_event_getdatum(gev);

    //     // Create an object of type GDP_EVENT that we'll return
    //     GDP_EVENT ev = new GDP_EVENT(this.ginh, datum, type);

    //     // Free the C data-structure
    //     Gdp20Library.INSTANCE.gdp_event_free(gev);

    //     return ev;
    // }

    ///////////////////////////////////////////////////////////////////
    ////                   private methods                         ////

    /** If ginh is null, then throw an exception stating that close()
     *  was probably called.
     */   
    private static void _checkGclh(Pointer ginh) {
        if (ginh == null) {
            throw new NullPointerException("The pointer to the C gdp_gin_t structure was null."
                                           + "Perhaps close() was called?  "
                                           + "See https://gdp.cs.berkeley.edu/redmine/issues/83");
        }
    }


    ///////////////////////////////////////////////////////////////////
    ////                   private variables                       ////

    /**
     * A global list of objects, which is useful for get_next_event().
     */
    private static HashMap<Pointer, Object> _allGclhs = 
            new HashMap<Pointer, Object>();

}
