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
 * GCL: GDP Channel Log
 * A class that represents a log. The name GCL is for historical reasons.
 * This mimics the Python wrapper around the GDP C-library. However, be
 * aware that this Java wrapper may not make the latest features available.
 * 
 * @author Nitesh Mor, based on Christopher Brooks' early implementation.
 *
 */
public class GDP_GCL {

    /**
     * internal 256 bit name for the log
     */
    public byte[] gclname;

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

    /**
     * A global list of objects. Useful for get_next_event
     */
    public static HashMap<Pointer, Object> object_dir = 
            new HashMap<Pointer, Object>();
    
    /**
     * Create a GCL. Note that this is a static method. 
     * 
     * @param logName   Name of the log to be created
     * @param logdName  Name of the log server where this should be 
     *                  placed.
     * @param metadata  Metadata to be added to the log. 
     */
    
    public static void create(GDP_NAME logName, GDP_NAME logdName, 
                    Map<Integer, byte[]> metadata) {
        
        EP_STAT estat;
        
        // Get the 256-bit internal names for logd and log
        ByteBuffer logdNameInternal = ByteBuffer.wrap(logdName.internal_name());
        ByteBuffer logNameInternal = ByteBuffer.wrap(logName.internal_name());
        
        // Just create a throwaway pointer.
        Pointer tmpPtr = null;
        
        // metadata processing
        
        GDP_GCLMD m = new GDP_GCLMD();
        for (int k: metadata.keySet()) {
            m.add(k, metadata.get(k));
        }
        
        estat = Gdp06Library.INSTANCE.gdp_gcl_create(logNameInternal, logdNameInternal,
                        m.gdp_gclmd_ptr, new PointerByReference(tmpPtr));
        
        GDP.check_EP_STAT(estat);
        
        return;
    }
    

    /**
     * Create a GCL (with empty metadata). Note that this is a static method    
     * @param logName   Name of the log
     * @param logdName  Name of the logserver that should host this log
     */
    public static void create(GDP_NAME logName, GDP_NAME logdName) {
        create (logdName, logName, new HashMap<Integer, byte[]>());
    }

    /** 
     * Initialize a new GCL object.
     * No signatures support, yet. TODO
     * 
     * @param name   Name of the log.
     * @param iomode Should this be opened read only, read-append, append-only
     */
    public GDP_GCL(GDP_NAME name, GDP_MODE iomode) {

        EP_STAT estat;
        
        PointerByReference gclhByReference = new PointerByReference();
        this.iomode = iomode;
        this.gclname = name.internal_name();
        
        // open the GCL
        estat = Gdp06Library.INSTANCE.gdp_gcl_open(ByteBuffer.wrap(this.gclname), 
                            iomode.ordinal(), (PointerByReference) null, 
                            gclhByReference);
        GDP.check_EP_STAT(estat);

        // associate the C pointer to this object.
        this.gclh = gclhByReference.getValue();
        assert this.gclh != null;

        // Add ourselves to the global map of pointers=>objects 
        object_dir.put(this.gclh, this);
    }

    public void finalize() {
        
        // remove ourselves from the global list
        object_dir.remove(this.gclh);
        
        // free the associated gdp_gcl_t
        Gdp06Library.INSTANCE.gdp_gcl_close(this.gclh);
        
    }

    /** 
     * Create a new GCL.
     * This method is a convenience method used to create a GCL
     * when the GDP_MODE enum is not available, such as when
     * creating a GCL from JavaScript.
     *
     * @param name   Name of the log.
     * @param iomode Opening mode (0: for internal use only, 1: read-only, 2: read-append, 3: append-only)
     */
    public static GDP_GCL newGCL(GDP_NAME name, int iomode) {
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
        return new GDP_GCL(name, gdp_mode);
    }

    /**
     * Read a record by record number.
     * 
     * @param recno Record number to be read
     * @return A hashmap containing the data just read
     */
    public HashMap<String, Object> read(long recno) {
        EP_STAT estat;
        GDP_DATUM datum = new GDP_DATUM();
                
        // get the datum populated
        estat = Gdp06Library.INSTANCE.gdp_gcl_read(this.gclh, recno, 
                                datum.gdp_datum_ptr);
        GDP.check_EP_STAT(estat);

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
     * Append data to a log. This will create a new record in the log.
     * 
     * @param datum_dict    A dictionary containing the key "data". The value
     *                      associated should be a byte[] containing the data
     *                      to be appended.
     */
    public void append(Map<String, Object>datum_dict) {

        EP_STAT estat;
        GDP_DATUM datum = new GDP_DATUM();
        
        Object data = datum_dict.get("data");
        datum.setbuf((byte[]) data);
        
        estat = Gdp06Library.INSTANCE.gdp_gcl_append(this.gclh,
                                datum.gdp_datum_ptr);
        GDP.check_EP_STAT(estat);

        return;
    }

    /**
     * Append data to a log. This will create a new record in the log.
     * 
     * @param data  Data that should be appended
     */
    public void append(byte[] data) {
        
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
     */
    public void append_async(Map<String, Object>datum_dict) {

        EP_STAT estat;
        GDP_DATUM datum = new GDP_DATUM();
        
        Object data = datum_dict.get("data");
        datum.setbuf((byte[]) data);
        
        estat = Gdp06Library.INSTANCE.gdp_gcl_append_async(this.gclh, 
                                datum.gdp_datum_ptr, null, null);
        GDP.check_EP_STAT(estat);

        return;
    }


    /**
     * Append data to a log, asynchronously. This will create a new record in the log.
     * 
     * @param data  Data that should be appended
     */
    public void append_async(byte[] data) {
        
        HashMap<String, Object> datum = new HashMap<String, Object>();
        datum.put("data", data);
        
        this.append_async(datum);
    }


    /** 
     * Start a subscription to a log.
     * See the documentation in C library for examples.
     * 
     * @param firstrec  The record num to start the subscription from
     * @param numrecs   Max number of records to be returned. 0 => infinite
     * @param timeout   Timeout for this subscription
     */
    public void subscribe(long firstrec, int numrecs, EP_TIME_SPEC timeout) {

        EP_STAT estat;
        estat = Gdp06Library.INSTANCE
                    .gdp_gcl_subscribe(this.gclh, firstrec, numrecs,
                                        timeout, null, null);

        GDP.check_EP_STAT(estat);

        return;
    }

    /** 
     * Multiread: Similar to subscription, but for existing data
     * only. Not for any future records
     * See the documentation in C library for examples.
     * 
     * @param firstrec  The record num to start reading from
     * @param numrecs   Max number of records to be returned. 
     */
    public void multiread(long firstrec, int numrecs) {

        EP_STAT estat;
        estat = Gdp06Library.INSTANCE
                    .gdp_gcl_multiread(this.gclh, firstrec, numrecs,
                                        null, null);

        GDP.check_EP_STAT(estat);

        return;
    }

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
    //     if (ev.type == Gdp06Library.INSTANCE.GDP_EVENT_DATA) {
    //         return ev.datum.data;
    //     } else {
    //         return null;
    //     }

    // }
    
    public static HashMap<String, Object> _helper_get_next_event(
                            Pointer gclh, EP_TIME_SPEC timeout) {
        
        // Get the event pointer. gclh can be null.
        PointerByReference gdp_event_ptr = Gdp06Library.INSTANCE
                            .gdp_event_next(gclh, timeout);

        // Get the data associated with this event
        int type = Gdp06Library.INSTANCE.gdp_event_gettype(gdp_event_ptr);
        PointerByReference datum_ptr = Gdp06Library.INSTANCE
                            .gdp_event_getdatum(gdp_event_ptr);
        EP_STAT event_ep_stat = Gdp06Library.INSTANCE
                            .gdp_event_getstat(gdp_event_ptr);
        PointerByReference _gclhByReference = Gdp06Library.INSTANCE
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
        gdp_event.put("gcl_handle", object_dir.get(_gclh));
        gdp_event.put("datum", datum_dict);
        gdp_event.put("type", type);
        gdp_event.put("stat", event_ep_stat);
        
        // free the event structure
        Gdp06Library.INSTANCE.gdp_event_free(gdp_event_ptr);
        
        return gdp_event;
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
            HashMap<String, Object> tmp = _helper_get_next_event(
                    obj.gclh, timeout);
            assert tmp.get("gcl_handle") == obj;

            return tmp;
        }
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
    //     PointerByReference gev = Gdp06Library.INSTANCE
    //                                 .gdp_event_next(this.gclh, timeout);

    //     // Get the data associated with this event
    //     int type = Gdp06Library.INSTANCE.gdp_event_gettype(gev);
    //     PointerByReference datum = Gdp06Library.INSTANCE.gdp_event_getdatum(gev);

    //     // Create an object of type GDP_EVENT that we'll return
    //     GDP_EVENT ev = new GDP_EVENT(this.gclh, datum, type);

    //     // Free the C data-structure
    //     Gdp06Library.INSTANCE.gdp_event_free(gev);

    //     return ev;
    // }
}
