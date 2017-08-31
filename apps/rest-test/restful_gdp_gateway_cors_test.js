// bash-3.2$ ./restful_gcl_create.py 
//
// ==== PUT GCL Response Page:
// STATUS:
// 201
// HEADERS:
// {'Access-Control-Allow-Methods': 'GET, POST, PUT, OPTIONS', 'Access-Control-Max-Age': '86400', 'Transfer-Encoding': 'chunked', 'Server': 'lighttpd/1.4.35', 'Date': 'Thu, 31 Aug 2017 16:21:49 GMT', 'Access-Control-Allow-Origin': '*', 'Access-Control-Allow-Headers': 'Authorization, Content-Type', 'Content-Type': 'application/json'}
// CONTENT:
// {
//     "gdplogd_name": "edu.berkeley.eecs.gdp-04.gdplogd",
//     "gcl_name": "wnRXJzfgkHhtbtst0AWU4pFR1HPOOKvPJF7Gpqt1ewA"
// }
//
// ==== END PUT GCL Response Page
//
// Info: created GCL wnRXJzfgkHhtbtst0AWU4pFR1HPOOKvPJF7Gpqt1ewA
// bash-3.2$ 

$(document).ready(function() {

    document.getElementById("method_1").innerHTML = "POST";
    var url_post = "https://gdp-rest-01.eecs.berkeley.edu/gdp/v1/gcl/" +
    	"wnRXJzfgkHhtbtst0AWU4pFR1HPOOKvPJF7Gpqt1ewA";
    document.getElementById("request_1").innerHTML = url_post;
    var time1 = new Date().toISOString();
    document.getElementById("time_1").innerHTML = time1;

    $.ajax({
	
    	beforeSend : function(xhr) {
    	    xhr.setRequestHeader('Authorization',
				 'Basic ZWNkZW1vOnRlcnJhc3dhcm0=');
    	},
    	method: "POST",
    	url: url_post,
    	contentType: "application/json",
    	data: JSON.stringify({
    	    "gcl" : "wnRXJzfgkHhtbtst0AWU4pFR1HPOOKvPJF7Gpqt1ewA",
    	    "testcase" : "POST append record " + time1
    	}),
    	error : function(xhr, textStatus, errorThrown) {
    	    $('#responsecode_1').text(xhr.status)
    	    $('#response_1').text(textStatus);
    	},
    	success : function(data, status, xhr) {
    	    $('#responsecode_1').text(xhr.status)
    	    $('#response_1').text(JSON.stringify(data));
    	}
    }) .done (function() {

    	var url_get_last = "https://gdp-rest-01.eecs.berkeley.edu/gdp/v1/gcl/" +
    	    "wnRXJzfgkHhtbtst0AWU4pFR1HPOOKvPJF7Gpqt1ewA?recno=-1";
    	document.getElementById("method_2").innerHTML = "GET";
    	document.getElementById("request_2").innerHTML = url_get_last;

    	$.ajax({
	    
    	    beforeSend : function(xhr) {
    		xhr.setRequestHeader('Authorization',
				     'Basic ZWNkZW1vOnRlcnJhc3dhcm0=');
    	    },
    	    url: url_get_last,		    
    	    error : function(xhr, textStatus, errorThrown) {
    		$('#responsecode_2').text(xhr.status)
    		$('#response_2').text(textStatus);
    	    },
    	    success : function(data, status, xhr) {
    		$('#responsecode_2').text(xhr.status)
    		$('#response_2').text(JSON.stringify(data));
    	    }
    	});
    });
});
