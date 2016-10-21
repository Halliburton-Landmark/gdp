// All javascript functions go here...


//BaseURL = "http://gdp.cs.berkeley.edu:5006/main?"
BaseURL = "http://localhost:5006/main?"

// from https://stackoverflow.com/questions/111529/how-to-create-query-parameters-in-javascript
function EncodeQueryData(data) {
   var ret = [];
   for (var d in data)
      ret.push(encodeURIComponent(d) + "=" + encodeURIComponent(data[d]));
   return ret.join("&");
}


function plot(form) {

    var plottime = form.plottime.value;
    var params = {};
    params['log_0'] = "edu.berkeley.eecs.bwrc.device.c098e5300054";
    params['log_1'] = "edu.berkeley.eecs.bwrc.device.c098e5300009";

    // Get start and end time
    params['start'] = 1477000000;
    params['end'] = params['start'] + parseInt(plottime);

    // height and width of individual plots
    params['height'] = 200;
    params['width'] = parseInt(document.body.clientWidth/2);

    // We only need temperature
    params['plot_0_title'] = "Temperature comparison for server room";
    params['plot_0_keys'] = "temperature_celcius";

    var urlString = BaseURL + EncodeQueryData(params);

    document.getElementById('dashboard_div').innerHTML =
                    "<iframe src=\"" + urlString + "\"></iframe>";

}

