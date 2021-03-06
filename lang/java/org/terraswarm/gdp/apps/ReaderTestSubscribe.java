/* A simple test.

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

package org.terraswarm.gdp.apps;

import org.terraswarm.gdp.*;
import java.util.Date;
import java.util.HashMap;

/** A simple test.
 */
public class ReaderTestSubscribe {

    public static void main(String[] args) {

        try {
            GDP.gdp_init();
            GDP.dbg_set("*=10");

            if (args.length<1) {
                System.out.println("Usage: <logname>");
                return;
            }

            String logname = args[0];
            GDP_NAME gn = new GDP_NAME(logname);

            System.out.println("Opening object");
            GDP_GIN g = new GDP_GIN(gn, GDP_GIN.GDP_MODE.RA);

            g.subscribe_by_recno(0, 0);
            for (;;) {
                GDP_EVENT event = GDP_GIN.get_next_event(null, null);
                if (event.gettype() == 2) { break; }
                GDP_DATUM datum = event.getdatum();
                System.out.print(new String((byte[]) datum.getbuf(), "UTF-8"));
                System.out.println();
            }
        } catch (Throwable throwable) {
            System.err.println(throwable);
            throwable.printStackTrace();
        }

    }
}
