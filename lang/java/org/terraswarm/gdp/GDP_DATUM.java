package org.terraswarm.gdp; 
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.util.Date;
import java.util.Map;
import java.awt.PointerInfo;
import java.lang.Exception;


import com.sun.jna.Native;
import com.sun.jna.Memory;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;
import org.terraswarm.gdp.NativeSize; // Fixed by cxh in makefile.

/**
 * A class that represents a GDP Datum. The original C GDP Datum
 * may create confusion since data is stored in a buffer (which
 * unsurprisingly acts like a buffer).
 * <p>
 * This is for internal use only, for users the GDP DATUM is more
 * like a dictionary, represented with a HashMap.
 * <p>
 * TODO Make sure there are no memory leaks. Implement a destructor.
 * @author nitesh mor
 *
 */
class GDP_DATUM {
        
    // The associated C data structure.
    public PointerByReference gdp_datum_ptr = null;

    // to keep track of whether we need to free the associated C memory.
    private boolean did_i_create_it = false;
    
    /**
     * Create a new dataum.
     */
    public GDP_DATUM() {
        this.gdp_datum_ptr = Gdp05Library.INSTANCE.gdp_datum_new();
        did_i_create_it = true;
        return;
    }
    
    /**
     * Use an already allocated C data structure.
     * @param p Pointer to existing C memory of type gdp_datum_t
     */    
    public GDP_DATUM(PointerByReference d) {
        this.gdp_datum_ptr = d;
        did_i_create_it = false;
        return;
    }
    
    /**
     * If we allocated memory ourselves, free it.
     */
    public void finalize() { 
        if (did_i_create_it == true) {
            Gdp05Library.INSTANCE.gdp_datum_free(this.gdp_datum_ptr);
        }
    }
    
    /**
     * Get the associated record number.
     * @return Record number
     */
    public long getrecno() {
        long recno = Gdp05Library.INSTANCE.gdp_datum_getrecno(
                                        this.gdp_datum_ptr);
        return recno;
    }
    
    /**
     * Get the timestamp.
     * @return timestamp associated with the C data structure
     */
    public EP_TIME_SPEC getts() {
        EP_TIME_SPEC ts = new EP_TIME_SPEC();
        Gdp05Library.INSTANCE.gdp_datum_getts(this.gdp_datum_ptr, ts);
        return ts;
    }
    
    /**
     * Get the length of data in the bufffer.
     * @return Length of buffer
     */
    public NativeSize getdlen() {
        PointerByReference buf = Gdp05Library.INSTANCE.gdp_datum_getbuf(
                                        this.gdp_datum_ptr);
        NativeSize len = Gdp05Library.INSTANCE.gdp_buf_getlength(buf);
        return len;
    }
    
    /**
     * Get the data in the buffer. Internally, queries the buffer and
     * effectively drains it as well.
     * @return the bytes in the buffer or null if the pointer to the
     * buffer is null.
     */
    public byte[] getbuf(){
        PointerByReference buf = Gdp05Library.INSTANCE.gdp_datum_getbuf(
                this.gdp_datum_ptr);
        NativeSize len = Gdp05Library.INSTANCE.gdp_buf_getlength(buf);
        Pointer bufptr = Gdp05Library.INSTANCE.gdp_buf_getptr(buf, len);
        if (bufptr == null) {
            return null;
        }
        byte[] bytes = bufptr.getByteArray(0, len.intValue());
        return bytes;
    }
    
    
    /**
     * Set the buffer to specified bytes.
     * @param data The data
     */
    public void setbuf(byte[] data){
        
        // Create a C string that is a copy (?) of the Java String.
        Memory memory = new Memory(data.length);
        // FIXME: not sure about alignment.
        Memory alignedMemory = memory.align(4);
        memory.clear();
        Pointer pointer = alignedMemory.share(0);
        
        for (int i=0; i<data.length; i++) {
            pointer.setByte(i,data[i]);
        }

        // Now feed this data into the gdp buffer
        PointerByReference dbuf = Gdp05Library.INSTANCE.gdp_datum_getbuf(
                                    this.gdp_datum_ptr);
        Gdp05Library.INSTANCE.gdp_buf_write(dbuf, pointer, 
                                new NativeSize(data.length));
    }
}
