package org.terraswarm.gdp.apps;

import org.terraswarm.gdp.*;
import java.util.*;
import java.text.SimpleDateFormat;

import com.sun.jna.Memory;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;
import org.terraswarm.gdp.NativeSize;

/**
 * A simple example to demonstrate and verify log creation
 * @author nitesh mor
 *
 */


public class CreateTestWithKey {

    public static void main(String[] args) throws GDPException{

        // Just to check any error output
        GDP.dbg_set("*=10");

        if (args.length<3) { // usage
            System.out.print("Usage: <logname> <keyfile-name> <server name>");
            System.out.println();
            return;
        }

        GDP_NAME logName = new GDP_NAME(args[0]);
        String keyfile = args[1];
        GDP_NAME logdName = new GDP_NAME(args[2]);

        Map<Integer, byte[]> m = new HashMap<Integer, byte[]>();
        m.put(GDP_MD.GDP_MD_XID, args[0].getBytes());

        // This is where we read the keyfile into memory.
        PointerByReference EPKeyPtr = Gdp21Library.INSTANCE.ep_crypto_key_read_file(
                            keyfile,
                            Gdp21Library.INSTANCE.EP_CRYPTO_KEYFORM_PEM,
                            Gdp21Library.INSTANCE.EP_CRYPTO_F_SECRET);

        // We need to write the public part in a buffer, which will be
        // placed in the metadata. The actual formatting of data is
        // not really exposed to programmers for the moment (i.e. documentaed);
        // this is based on `gdp-create.c`.

        Pointer PubKey = new Memory(Gdp21Library.INSTANCE.EP_CRYPTO_MAX_DER);
        EP_STAT estat = Gdp21Library.INSTANCE.ep_crypto_key_write_mem(EPKeyPtr, PubKey,
                            new NativeSize(Gdp21Library.INSTANCE.EP_CRYPTO_MAX_DER),
                            Gdp21Library.INSTANCE.EP_CRYPTO_KEYFORM_DER,
                            0,
                            Pointer.NULL,
                            Gdp21Library.INSTANCE.EP_CRYPTO_F_PUBLIC);
        // number of bytes written to memory
        int keylen = estat.code;

        byte[] buf = new byte[4 + keylen];
        buf[0] = (byte) Gdp21Library.INSTANCE.EP_CRYPTO_MD_SHA256;
        buf[1] = (byte) Gdp21Library.INSTANCE.ep_crypto_keytype_fromkey(EPKeyPtr);

        // I don't quite understand the significance of the keylength field.
        // It's set to 0, and is ignored for the most part.
        buf[2] = 0;
        buf[3] = 0;

        // add these bytes to the metadata.
        for (int i=0; i<keylen; i++)
            buf[i+4] = PubKey.getByte(i);
        m.put(GDP_MD.GDP_MD_PUBKEY, buf);
        
        System.out.println("Creating log " + args[0]);
        
        GDP_GIN.create(logName, logdName, m);

        // XXX
        // The user should copy the key to the appropriate filename, so that C
        // library doesn't run into problems. Typically, this is `printable-name.pem`
        return;
        
    }
}
