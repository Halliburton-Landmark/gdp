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
 * A class to hold utility functions for the GDP. For the lack of a 
 * better name and to keep things concise, we just call it GDP.
 * Most (if not all) members of this class should be static. The 
 * auto-generated constants in Gdp05Library.java that should be made
 * available to the users should also ideally be copied here.
 *  
 * @author nitesh mor
 *
 */
public class GDP {

    // some constants
    public static final int GDP_GCLMD_XID     = 0x00584944; 
    public static final int GDP_GCLMD_PUBKEY  = 0x00505542;
    public static final int GDP_GCLMD_CTIME   = 0x0043544D;      
    public static final int GDP_GCLMD_CID     = 0x00434944;
       
    /**
     * Initialize GDP library, with any parameters specified
     * in configuration files.
     */
    public static void gdp_init() {
        EP_STAT estat;
        estat = Gdp06Library.INSTANCE.gdp_init((Pointer)null);        
        check_EP_STAT(estat);        
       
    }
    
    /**
     * Initialize the GDP library
     * @param gdpRouter Address of GDP routers to connect to. Overrides any
     *      configuration settings.
     */
    public static void gdp_init(String gdpRouter) {
        EP_STAT estat;
        estat = Gdp06Library.INSTANCE.gdp_init(gdpRouter);
        check_EP_STAT(estat);
        
    }
    
    /** 
     * Set the debug levels for the EP/GDP C library
     * @param debug_level A string of the form X=Y, Y is an integer.
     */
    public static void dbg_set(String debug_level) {

        Gdp06Library.INSTANCE.ep_dbg_set(debug_level);
    }


    /** 
     * Error checking. For any GDP call that returns an EP_STAT 
     * structure, this is where we decode the integer return code.
     * <p>
     * TODO ideally should throw exceptions that ought to be caught
     */
    public static boolean check_EP_STAT(EP_STAT estat){

        int code = estat.code;
        int EP_STAT_SEVERITY = (code >>> Gdp06Library._EP_STAT_SEVSHIFT)
                & ((1 << Gdp06Library._EP_STAT_SEVBITS) - 1);

        return (EP_STAT_SEVERITY < Gdp06Library.EP_STAT_SEV_WARN);
    }

    
}
