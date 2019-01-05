package org.terraswarm.gdp.apps;

import org.terraswarm.gdp.*;
import java.util.*;
import java.text.SimpleDateFormat;

/**
 * A simple example to demonstrate and verify log creation
 * @author nitesh mor
 *
 */


public class CreateTest {

    public static void main(String[] args) throws GDPException{

        // Just to check any error output
        GDP.dbg_set("*=10");

        if (args.length<2) { // usage
            System.out.print("Usage: <logname> <server name>");
            System.out.println();
            return;
        }

        GDP_NAME logName = new GDP_NAME(args[0]);
        GDP_NAME logdName = new GDP_NAME(args[1]);

        Map<Integer, byte[]> m = new HashMap<Integer, byte[]>();
        m.put(GDP_MD.GDP_MD_XID, args[0].getBytes());
        
        System.out.println("Creating log " + args[0]);
        
        GDP_GIN.create(logName, logdName, m);
        
        return;
        
    }
}
