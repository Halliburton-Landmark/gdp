/* vim: set ai sw=4 sts=4 ts=4 : */

/*
** 
**	----- BEGIN LICENSE BLOCK -----
**	Copyright (c) 2015-2017, Electronics and Telecommunications 
**	Research Institute (ETRI). All rights reserved. 
**	Copyright (c) 2015-2017, Regents of the University of California.
**	All rights reserved.
**
**	Permission is hereby granted, without written agreement and without
**	license or royalty fees, to use, copy, modify, and distribute this
**	software and its documentation for any purpose, provided that the above
**	copyright notice and the following two paragraphs appear in all copies
**	of this software.
**
**	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
**	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
**	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
**	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
**	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
**	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
**	OR MODIFICATIONS.
**	----- END LICENSE BLOCK -----
*/

/*
**  KDS_PROTO -- ACTIONS for each command to Key distribution Service   
**
** written by Hwa Shin Moon, ETRI (angela.moon@berkeley.edu, hsmoon@etri.re.kr) 
** last modified : 2017.11.24 
*/ 

#ifndef	_KSD_KEY_INFO_H_
#define	_KSD_KEY_INFO_H_


/*
#include <sys/time.h>
#include <sys/queue.h>

#include <gdp/gdp.h>
#include <gdp/gdp_priv.h>
#include <hs/hs_errno.h>
#include <hs/hs_list.h>
#include <ep/ep_app.h>
#include <ac/acrule.h>
#include <gdp/gdp_event.h>
*/



/*
// Major Interface. 
// Process the request of key service creation of log writer
void process_key_creation_request();
// Process the subscription of key from the log writer. 
// Multiple logs can use the same log key. 
void process_key_subscription_from_writer();
// Process the request of key from the log readers. 
void process_key_read_request();
void process_key_subscription_from_reader(); 


// Internal Major functions  
// Periodically or at the event
// log key is changed... 
// when key is changed, key is forwarded to the writers & subscribed readers. 
void change_key();
// Periodically check.. 
// if time condition is met, key is changed... 
void check_key_state();

*/


#endif  //_KSD_KEY_INFO_H_

