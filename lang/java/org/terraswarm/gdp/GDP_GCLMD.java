package org.terraswarm.gdp; 
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.util.Date;
import java.util.HashMap;
import java.util.Map;
import java.awt.PointerInfo;
import java.lang.Exception;


import com.sun.jna.Native;
import com.sun.jna.Memory;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;
import org.terraswarm.gdp.NativeSize; // Fixed by cxh in makefile.

/**
 * A private data structure that represents gdp_gclmd_t
 * 
 * @author nitesh mor
 */

class GDP_GCLMD {
    
    // Pointer to the C structure
    public PointerByReference gdp_gclmd_ptr = null;
    
    // To keep track of whether we need to call free in destructor
    private boolean did_i_create_it = false;

    /**
     * Pass a pointer to an already allocated gdp_gclmd_t C structure.
     * @param d A pointer to an existing gdp_gclmd_t
     */
    public GDP_GCLMD(PointerByReference d) {
        this.gdp_gclmd_ptr = d;
        this.did_i_create_it = false;
        return;
    }
    
    /**
     * If called without an existing pointer, we allocated a new 
     * C gdp_gclmd_t structure.
     */
    public GDP_GCLMD() {
        this.gdp_gclmd_ptr = Gdp05Library.INSTANCE.gdp_gclmd_new(0);
        this.did_i_create_it = true;
    }
    
    /**
     * In case we allocated memory for the C gdp_gclmd_t ourselves, free it.
     */
    public void finalize() {        
        if (this.did_i_create_it) {
            Gdp05Library.INSTANCE.gdp_gclmd_free(this.gdp_gclmd_ptr);
        }
    }
    
    /**
     * Add a new entry to the metadata
     * @param gclmd_id  ID for the metadata. Internally a 32-bit unsigned integer
     * @param data  The corresponding data.
     */
    public void add(int gclmd_id, byte[] data) {
        
        // FIXME: figure out if we need to have a +1 for len
        NativeSize len = new NativeSize(data.length);

        // Create a C string that is a copy (?) of the Java String.
        Memory memory = new Memory(data.length+1);
        
        // FIXME: not sure about alignment.
        Memory alignedMemory = memory.align(4);
        memory.clear();
        Pointer pointer = alignedMemory.share(0);

        for (int i=0; i<data.length; i++) {
            pointer.setByte(i,data[i]);
        }
                
        Gdp05Library.INSTANCE.gdp_gclmd_add(this.gdp_gclmd_ptr, gclmd_id,
                len, pointer);
    }

    
    /**
     * Get a metadata entry by index. This is not searching by a gclmd_id. For
     * that, look at find.
     * @param index The particular metadata entry we are looking for
     * @return  Returns a Key-Value pair containing the requested index. 
     *          A hashMap seems to be the best choice, since there's no tuple
     *          type in Java.
     */ 
    public HashMap<Integer, byte[]> get(int index){

        // Pointer to hold the gclmd_id
        IntBuffer gclmd_id_ptr = IntBuffer.allocate(1);
        
        // to hold the length of the buffer
        NativeSize len = new NativeSize();
        
        // Pointer to the buffer to hold the value
        PointerByReference p = new PointerByReference();
        
        Gdp05Library.INSTANCE.gdp_gclmd_get(this.gdp_gclmd_ptr, 
                index, gclmd_id_ptr, new NativeSizeByReference(len), p);
        
        // get the gclmd_id
        int gclmd_id = gclmd_id_ptr.get(0);
        
        Pointer pointer = p.getValue();
        byte[] val = pointer.getByteArray(0, len.intValue());
        
        HashMap<Integer, byte[]> ret = new HashMap<Integer, byte[]>();
        
        ret.put(gclmd_id, val);
        
        return ret;
    }
    
    /**
     * Find a particular metadata entry by looking up gclmd_id
     * @param gclmd_id ID we are searching for
     * @return the value associated with <code>gclmd_id</code> 
     */
    public byte[] find(int gclmd_id) {

        // to hold the length of the buffer
        NativeSize len = new NativeSize();
        // Pointer to the buffer to hold the value
        PointerByReference p = new PointerByReference();
        
        Gdp05Library.INSTANCE.gdp_gclmd_find(this.gdp_gclmd_ptr, 
                gclmd_id, new NativeSizeByReference(len), p);
        
        Pointer pointer = p.getValue();
        byte[] bytes = pointer.getByteArray(0, len.intValue());
        return bytes;
        
    }
}
