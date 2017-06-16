/* A Global Data Plane (GDP) Channel Log (GCL).

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
 * A Global Data Plane (GDP) Channel Log (GCL).
 *
 * <p>This Java wrapper may not have the latest features in the
 * underlying C implementation.</p>
 * 
 * @author Nitesh Mor, Christopher Brooks
 */
public class GDP_GCL {
    
    // The underscores in the names of public methods and variables should
    // be preserved so that they match the underlying C-based gdp code.

    /** 
     * Initialize a new Global Data Plane Channel Log (GCL) object.
     *
     * Signatures are not yet supported. 
     *
     * @param name   Name of the log, which will be created if necessary.
     * @param iomode Should this be opened read only, read-append,
     * append-only.  See {@link #GDP_MODE}.
     * @param logdName  Name of the log server where this should be 
     *                  placed if it does not yet exist.
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp07Library.EP_STAT_SEV_WARN.
     */
    public GDP_GCL(GDP_NAME name, GDP_MODE iomode, GDP_NAME logdName) throws GDPException {
        // FIXME: No signatures support.

	System.out.println("GDP_GCL.java: GDP_GCL(" + name +
			   "(" + new String(name.printable_name()) +
			   "), " + iomode + ", " + logdName +
			   "(" + new String(logdName.printable_name()) +
			   ")");
        EP_STAT estat;
        
        PointerByReference gclhByReference = new PointerByReference();
        this.iomode = iomode;
        this.gclName = name.internal_name();
        
        // Open the GCL.
	GDP.dbg_set("*=20");
        estat = Gdp07Library.INSTANCE.gdp_gcl_open(ByteBuffer.wrap(this.gclName), 
                            iomode.ordinal(), (PointerByReference) null, 
                            gclhByReference);
	if (!GDP.check_EP_STAT(estat)) {
	    System.out.println("GDP_GCL: gdp_gcl_open() failed, trying to create the log and call gdp_gcl_open() again.");
	    GDP_GCL.create(name, logdName);
	    estat = Gdp07Library.INSTANCE.gdp_gcl_open(ByteBuffer.wrap(this.gclName), 
						       iomode.ordinal(), (PointerByReference) null, 
						       gclhByReference);
	}
        GDP.check_EP_STAT(estat, "gdp_gcl_open(" +
			  gclName + "(" + new String(name.printable_name(),0) + "), " +
			  iomode.ordinal() + ", null, " +
			  gclhByReference + ") failed.");

        // Associate the C pointer to this object.
        gclh = gclhByReference.getValue();
        assert this.gclh != null;

        // Add ourselves to the global map of pointers=>objects 
        _allGclhs.put(this.gclh, this);
    }

    ///////////////////////////////////////////////////////////////////
    ////                   public fields                           ////

    /**
     * The internal 256 bit name for the log.
     */
    public byte[] gclName;

    /**
     * A pointer to the C gdp_gcl_t structure
     */
    public Pointer gclh = null;

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
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp07Library.EP_STAT_SEV_WARN.
     */
    public void append(Map<String, Object>datum_dict) throws GDPException {

        EP_STAT estat;
        GDP_DATUM datum = new GDP_DATUM();
        
        Object data = datum_dict.get("data");
        datum.setbuf((byte[]) data);
        
        _checkGclh(gclh);
        estat = Gdp07Library.INSTANCE.gdp_gcl_append(gclh,
                                datum.gdp_datum_ptr);
        GDP.check_EP_STAT(estat, "gdp_gcl_append(" + gclh + ", " + datum.gdp_datum_ptr + ") failed.");

        return;
    }

    /**
     * Append data to a log. This will create a new record in the log.
     * 
     * @param data  Data that should be appended
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp07Library.EP_STAT_SEV_WARN.
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
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp07Library.EP_STAT_SEV_WARN.
     */
    public void append_async(Map<String, Object>datum_dict) throws GDPException {

        EP_STAT estat;
        GDP_DATUM datum = new GDP_DATUM();
        
        Object data = datum_dict.get("data");
        datum.setbuf((byte[]) data);
        
        _checkGclh(gclh);
        estat = Gdp07Library.INSTANCE.gdp_gcl_append_async(gclh, 
                                datum.gdp_datum_ptr, null, null);
        GDP.check_EP_STAT(estat, "gdp_gcl_append_async(" + gclh + ", " + datum.gdp_datum_ptr + "null, null) failed.");

        return;
    }

    /**
     * Append data to a log, asynchronously. This will create a new record in the log.
     * 
     * @param data  Data that should be appended
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp07Library.EP_STAT_SEV_WARN.
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
        synchronized (gclh) {
            if (gclh != null) {
                // Remove ourselves from the global list.
                _allGclhs.remove(gclh);
                
                // Free the associated gdp_gcl_t.
                Gdp07Library.INSTANCE.gdp_gcl_close(gclh);
                gclh = null;
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
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp07Library.EP_STAT_SEV_WARN.
     */
    public static void create(GDP_NAME logName, GDP_NAME logdName, 
                    Map<Integer, byte[]> metadata) throws GDPException {
	System.out.println("GDP_GCL.java: create(" + logName + ", " + ", " + logdName + ", " + metadata + ")");
        EP_STAT estat;
        
        // Get the 256-bit internal names for log and logd
        ByteBuffer logNameInternal = ByteBuffer.wrap(logName.internal_name());
        ByteBuffer logdNameInternal = ByteBuffer.wrap(logdName.internal_name());
        
        // Just create a throwaway pointer.
        Pointer tmpPtr = null;
        
        // Metadata processing.
        GDP_GCLMD m = new GDP_GCLMD();
        for (int k: metadata.keySet()) {
            m.add(k, metadata.get(k));
        }
        
        estat = Gdp07Library.INSTANCE.gdp_gcl_create(logNameInternal, logdNameInternal,
                        m.gdp_gclmd_ptr, new PointerByReference(tmpPtr));
        
        GDP.check_EP_STAT(estat, "gdp_gcl_create(" + 
			  logNameInternal + ", " +
			  logdNameInternal + ", " +
			  m.gdp_gclmd_ptr + " new PointerByReference(" + tmpPtr + ")) failed.");
        
        return;
    }
    
    /**
     * Create a GCL (with empty metadata).
     * @param logName   Name of the log
     * @param logdName  Name of the logserver that should host this log
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp07Library.EP_STAT_SEV_WARN.
     */
    public static void create(String logName, String logdName) throws GDPException {
        GDP_GCL.create(new GDP_NAME(logName), new GDP_NAME(logdName));
    }
    /**
     * Create a GCL (with empty metadata).
     * @param logName   Name of the log
     * @param logdName  Name of the logserver that should host this log
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp07Library.EP_STAT_SEV_WARN.
     */
    public static void create(GDP_NAME logName, GDP_NAME logdName) throws GDPException {
        GDP_GCL.create(logName, logdName, new HashMap<Integer, byte[]>());
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
     * Multiread: Similar to subscription, but for existing data
     * only. Not for any future records
     * See the documentation in C library for examples.
     * 
     * @param firstrec  The record num to start reading from
     * @param numrecs   Max number of records to be returned. 
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp07Library.EP_STAT_SEV_WARN.
     */
    public void multiread(long firstrec, int numrecs) throws GDPException {

        EP_STAT estat;
        _checkGclh(gclh);
        estat = Gdp07Library.INSTANCE
                    .gdp_gcl_multiread(gclh, firstrec, numrecs,
                                        null, null);

        GDP.check_EP_STAT(estat, "gdp_gcl_multiread() failed.");

        return;
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
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp07Library.EP_STAT_SEV_WARN.
     */
    public static GDP_GCL newGCL(GDP_NAME name, int iomode, GDP_NAME logdName) throws GDPException {
	System.out.println("GDP_GCL.java: newGCL(" + name + ", " + iomode + ", " + logdName + ")");
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
        return new GDP_GCL(name, gdp_mode, logdName);
    }

    /**
     * Read a record by record number.
     * 
     * @param recno Record number to be read
     * @return A hashmap containing the data just read
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp07Library.EP_STAT_SEV_WARN.
     */
    public HashMap<String, Object> read(long recno) throws GDPException {
        EP_STAT estat;
        GDP_DATUM datum = new GDP_DATUM();
                
        // Get the datum populated.
        _checkGclh(gclh);
        estat = Gdp07Library.INSTANCE.gdp_gcl_read(gclh, recno, 
                                datum.gdp_datum_ptr);
        GDP.check_EP_STAT(estat, "gdp_gcl_read() failed.");

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
     * Start a subscription to a log.
     * See the documentation in C library for examples.
     * 
     * @param firstrec  The record num to start the subscription from
     * @param numrecs   Max number of records to be returned. 0 => infinite
     * @param timeout   Timeout for this subscription
     * @exception GDPException If a GDP C function returns a code great than or equal to Gdp07Library.EP_STAT_SEV_WARN.
     */
    public void subscribe(long firstrec, int numrecs, EP_TIME_SPEC timeout) throws GDPException {

        EP_STAT estat;
        _checkGclh(gclh);
        estat = Gdp07Library.INSTANCE
                    .gdp_gcl_subscribe(gclh, firstrec, numrecs,
                                        timeout, null, null);

        GDP.check_EP_STAT(estat, "gdp_gcl_subscribe() failed.");

        return;
    }

    /** 
     * Get the next event.
     * 
     * @param timeout_msec  Time (in ms) for which to block. Can be used 
     *                      to block eternally as well.
     */
    public static HashMap<String, Object> get_next_event(
                        GDP_GCL obj, int timeout_msec) {
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
                        GDP_GCL obj, EP_TIME_SPEC timeout) {
        if (obj == null) {
            return _helper_get_next_event(null, timeout);
        } else {
            _checkGclh(obj.gclh);
            HashMap<String, Object> tmp = _helper_get_next_event(
                    obj.gclh, timeout);
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
    //     if (ev.type == Gdp07Library.INSTANCE.GDP_EVENT_DATA) {
    //         return ev.datum.data;
    //     } else {
    //         return null;
    //     }

    // }
    
    public static HashMap<String, Object> _helper_get_next_event(
                            Pointer gclh, EP_TIME_SPEC timeout) {
        
        // Get the event pointer. gclh can be null.
        PointerByReference gdp_event_ptr = Gdp07Library.INSTANCE
                            .gdp_event_next(gclh, timeout);

        if (gdp_event_ptr == null) {
            return new HashMap<String, Object>();
            //throw new NullPointerException("gdp_event_next(" + gclh + ", " + timeout
            //        + ") returned null");
        }
        // Get the data associated with this event.
        // If gdp_event_gettype() is passed a NULL, then an assertion is thrown
        // and process exits.
        int type = Gdp07Library.INSTANCE.gdp_event_gettype(gdp_event_ptr);
        PointerByReference datum_ptr = Gdp07Library.INSTANCE
                            .gdp_event_getdatum(gdp_event_ptr);
        EP_STAT event_ep_stat = Gdp07Library.INSTANCE
                            .gdp_event_getstat(gdp_event_ptr);
        PointerByReference _gclhByReference = Gdp07Library.INSTANCE
                            .gdp_event_getgcl(gdp_event_ptr);
        Pointer _gclh = _gclhByReference.getValue();
                            
        
        GDP_DATUM datum = new GDP_DATUM(datum_ptr);
        // create a datum dictionary.
        HashMap<String, Object> datum_dict = new HashMap<String, Object>();
        datum_dict.put("recno", datum.getrecno());
        datum_dict.put("ts", datum.getts());
        datum_dict.put("data", datum.getbuf());
        // TODO Fix signatures
        
        HashMap<String, Object> gdp_event = new HashMap<String, Object>();
        gdp_event.put("gcl_handle", _allGclhs.get(_gclh));
        gdp_event.put("datum", datum_dict);
        gdp_event.put("type", type);
        gdp_event.put("stat", event_ep_stat);
        
        // free the event structure
        Gdp07Library.INSTANCE.gdp_event_free(gdp_event_ptr);
        
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
    //     PointerByReference gev = Gdp07Library.INSTANCE
    //                                 .gdp_event_next(this.gclh, timeout);

    //     // Get the data associated with this event
    //     int type = Gdp07Library.INSTANCE.gdp_event_gettype(gev);
    //     PointerByReference datum = Gdp07Library.INSTANCE.gdp_event_getdatum(gev);

    //     // Create an object of type GDP_EVENT that we'll return
    //     GDP_EVENT ev = new GDP_EVENT(this.gclh, datum, type);

    //     // Free the C data-structure
    //     Gdp07Library.INSTANCE.gdp_event_free(gev);

    //     return ev;
    // }

    ///////////////////////////////////////////////////////////////////
    ////                   private methods                         ////

    /** If gclh is null, then throw an exception stating that close()
     *  was probably called.
     */   
    private static void _checkGclh(Pointer gclh) {
        if (gclh == null) {
            throw new NullPointerException("The pointer to the C gdp_gcl_t structure was null."
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
