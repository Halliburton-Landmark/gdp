$(document).ready(function() {

    document.getElementById("method_1").innerHTML = "POST";
    var url_post = "https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl/" +
	"zeZ2RT9awTNHNYCJAVYS5tLJHsVXqb5DUVfXYR5lqr4";
    document.getElementById("request_1").innerHTML = url_post;
    var time1 = new Date().toISOString();
    document.getElementById("time_1").innerHTML = time1;

    $.ajax({
	
    	beforeSend : function(xhr) {
	    xhr.setRequestHeader('Authorization' , 'Basic ZWNkZW1vOnRlcnJhc3dhcm0=');
	},
    	method: "POST",
    	url: url_post,
    	contentType: "application/json",
    	data: JSON.stringify({
	    "gcl" : "zeZ2RT9awTNHNYCJAVYS5tLJHsVXqb5DUVfXYR5lqr4",
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

    	var url_get_last = "https://gdp-rest-01.eecs.berkeley.edu/gdp/v2/gcl/" +
    	    "zeZ2RT9awTNHNYCJAVYS5tLJHsVXqb5DUVfXYR5lqr4?recno=-1";
    	document.getElementById("method_2").innerHTML = "GET";
    	document.getElementById("request_2").innerHTML = url_get_last;

    	$.ajax({
	    
    	    beforeSend : function(xhr) {
    		xhr.setRequestHeader('Authorization' , 'Basic ZWNkZW1vOnRlcnJhc3dhcm0=');
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
