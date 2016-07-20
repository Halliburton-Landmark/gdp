// All javascript functions go here...
google.charts.load('current', {'packages':['corechart', 'controls']});

deviceData = {
        "logs": [
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e5300003", "type": "BLEES", "humanName": 'immersion BLEE: rear camera pillar'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e570002b", "type": "PowerBlade", "humanName": 'immersion PowerBlade: primary projector'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e570008b", "type": "PowerBlade", "humanName": 'immersion PowerBlade: secondary projector'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e5700088", "type": "PowerBlade", "humanName": 'immersion PowerBlade audience camera'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e570008e", "type": "PowerBlade", "humanName": 'immersion PowerBlade: speaker camera'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e590000a", "type": "Blink", "humanName": 'immersion Blink: front wall adjacent to screen'},


            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e530000a", "type": "BLEES", "humanName": 'swarm BLEE: inside entrance lobby'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e5900019", "type": "Blink", "humanName": 'swarm Blink: exit stair door near 490'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e590000b", "type": "Blink", "humanName": 'swarm Blink: inside entrance lobby'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e5900091", "type": "Blink", "humanName": 'swarm Blink: 4th floor elevator lobby'},


            { "logname": "edu.berkeley.eecs.bwrc.device.c098e5300009", "type": "BLEES", "humanName": 'bwrc BLEE: lab rear wall'},
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e5300036", "type": "BLEES", "humanName": 'bwrc BLEE: rear wall'},
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e5300054", "type": "BLEES", "humanName": 'bwrc BLEE: hot aisle'},
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e530005d", "type": "BLEES", "humanName": 'bwrc BLEE: mid wall'},
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e570008f", "type": "PowerBlade", "humanName": 'bwrc PowerBlade: utility power to sump pump'},
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e5700090", "type": "PowerBlade", "humanName": 'bwrc PowerBlade: power to air compressor'},
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e590001e", "type": "Blink", "humanName": 'bwrc Blink'}, 


            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e530002b", "type": "BLEES", "humanName": 'immersion BLEE: Chair 1'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e5300058", "type": "BLEES", "humanName": 'immersion BLEE: Chair 2'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e530005c", "type": "BLEES", "humanName": 'immersion BLEE: Chair 3'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e530005f", "type": "BLEES", "humanName": 'immersion BLEE: Chair 4'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e5300074", "type": "BLEES", "humanName": 'immersion BLEE: Chair 5'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e5300075", "type": "BLEES", "humanName": 'immersion BLEE: Chair 6'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e5300076", "type": "BLEES", "humanName": 'immersion BLEE: Chair 7'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e5300077", "type": "BLEES", "humanName": 'immersion BLEE: Chair 8'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e5300078", "type": "BLEES", "humanName": 'immersion BLEE: Chair 9'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e530007a", "type": "BLEES", "humanName": 'immersion BLEE: Chair 10'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e530007b", "type": "BLEES", "humanName": 'immersion BLEE: Chair 11'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e530007c", "type": "BLEES", "humanName": 'immersion BLEE: Chair 12'} ],
    };


function makeForm() {
    // Code for picking up a device
    var devicePickerString = "";
    for (var i=0; i<deviceData.logs.length; i++) {
        var log = deviceData.logs[i];
        devicePickerString += "<option value='" + log.logname + "'>" + log.humanName + "</option>";
    }
    document.getElementById('logname').innerHTML = devicePickerString;
    var timeNow = Date.now()
    document.getElementById('startTime').value = new Date(timeNow-3600000).toLocaleString();
    document.getElementById('endTime').value = new Date(timeNow).toLocaleString();
}

function plot(form) {
    var logname = form.logname.value;
    startTime = Date.parse(form.startTime.value)/1000.0;
    endTime = Date.parse(form.endTime.value)/1000.0;

    if (typeof dashboard != 'undefined') {
        for (var i=0; i<pChartArray.length; i++) {
            pChartArray[i].visualization.clearChart();
    	}
        pSlider.visualization.dispose();
        dashboard.dispose();
    }

    drawChart(logname, startTime, endTime);
}

function drawChart(logname, startTime, endTime) {

    var baseurl = "/datasource?";

    var query_string = "";
    query_string += "logname=" + String(logname) + "&"
    query_string += "startTime=" + String(startTime) + "&"
    query_string += "endTime=" + String(endTime)

    var query = new google.visualization.Query(baseurl+query_string);
    query.send(handleQueryResponse);
    document.getElementById('request_status').innerHTML = "<p>Request sent...</p>";
}

function handleQueryResponse(response) {

    data = response.getDataTable();
    if (data.getNumberOfRows() == 0) {
        document.getElementById('request_status').innerHTML = "<p>Sorry, no data for the time-range specified. Please pick a different time range.</p>";
        return;
    }

    document.getElementById('request_status').innerHTML = "<p>The first graph is an overview graph with controllable sliders to zoom in/out, followed by individual parameters plotted separately.</p>";

    dashboard = new google.visualization.Dashboard(
        document.getElementById('dashboard_div'));

    pSlider =  new google.visualization.ControlWrapper({
        'controlType': 'ChartRangeFilter',
        'containerId': 'control_div',
        'options': {
            'filterColumnLabel': 'time',
            //'ui': {'labelStacking': 'vertical'}
        },
        'state': {
            //'range': {'start': new Date(((startTime+endTime)/2)*1000),
            //          'end': new Date(endTime*1000)},
        }
    });

    pChartArray = [];
    var colors = ['black', 'blue', 'red', 'green', 'yellow', 'gray'];
    for (var i=1; i<data.getNumberOfColumns(); i++) {
        var pChart = new google.visualization.ChartWrapper({
        'chartType': 'LineChart',
        'containerId': 'chart_div_'+String(i),
        'view': {'columns': [0, i]},
        'options': {
            'colors': [colors[(i-1)%colors.length]],
            'title' : data.getColumnLabel(i),
            'pointSize': 3,
            //'curveType' : 'function',
            //'legend': { 'position': 'bottom'}
            }
        });

        pChartArray.push(pChart);
    }
    dashboard.bind(pSlider, pChartArray);
    dashboard.draw(data);
}

