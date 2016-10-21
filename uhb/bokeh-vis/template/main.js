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
    var lognames = form.lognames.value;
    var arr = lognames.split(/\r*\n\r*/);

    var params = {}
    for (i=0; i<arr.length; i++) {
        params["log_"+ String(i)] = arr[i].trim();
    }

    // Get start and end time
    var startTime = form.startTime.value;
    var endTime = form.endTime.value;
    params['start'] = startTime;
    params['end'] = endTime;

    // height and width of individual plots
    params['height'] = 200;
    params['width'] = document.body.clientWidth-400;

    // (Optional) Get titles and key names
    var plot_params = form.plot_params.value;
    // split lines
    var plot_arr = plot_params.split(/\r*\n\r*/);
    for (i=0; i<plot_arr.length; i++) {
        var tmp = plot_arr[i].split(',');
        if (tmp.length<2) {
            break;
        }
        params['plot_'+String(i)+'_title'] = tmp[0].trim();
        params['plot_'+String(i)+'_keys'] = tmp[1].trim();
    }

    var urlString = BaseURL + EncodeQueryData(params);

    document.getElementById('generatedURL').innerHTML = 
            "<p>Here's the URL string that was generated, and will" +
            " be displayed in an iframe below: </p>" + 
            "<code>" + urlString + "</code>" +
            "<p>You can embed a URL string like this in any of your" +
            " other applications as an IFrame, and it should work just" +
            " the same way it works here.</p>";

    if (lognames.length == 0 || startTime.length == 0 || endTime.length == 0) {
        document.getElementById('dashboard_div').innerHTML =
            "<p><blockquote>" +
            " Okay, I did not make this idiot-proof. The" +
            " point of this is to enable you create embeddable plots" +
            " from your existing data. Now, go and put together your" + 
            " parameters properly, and start visualizing." +
            "</blockquote></p>";
    } else {
        document.getElementById('dashboard_div').innerHTML = "<iframe src=\"" + urlString + "\"></iframe>";
    }

}

