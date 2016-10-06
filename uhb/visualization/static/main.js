// All javascript functions go here...
google.charts.load('current', {'packages':['corechart', 'controls']});

deviceData = {
        "logs": [
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e5300003", "type": "BLEES", "humanName": 'immersion BLEE: rear camera pillar'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e570002b", "type": "PowerBlade", "humanName": 'immersion PowerBlade: primary projector'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e570008b", "type": "PowerBlade", "humanName": 'immersion PowerBlade: secondary projector'},
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e5700088", "type": "PowerBlade", "humanName": 'immersion PowerBlade: audience camera'},
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
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e590001e", "type": "Blink", "humanName": 'bwrc Blink: server room'}, 

//added sept. 21: bwrc office area
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e53000a5", "type": "BLEES", "humanName": 'bwrc BLEE: main seating area'},
//            { "logname": "edu.berkeley.eecs.bwrc.device.c098e53000a3", "type": "BLEES", "humanName": 'bwrc BLEE: conference room (rabaey)'},
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e53000a4", "type": "BLEES", "humanName": 'bwrc BLEE: admin office'},
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e53000a2", "type": "BLEES", "humanName": 'bwrc BLEE: conference room (brodersen)'},
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e53000a1", "type": "BLEES", "humanName": 'bwrc BLEE: ken\'s office'},
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e53000a0", "type": "BLEES", "humanName": 'bwrc BLEE: brian office above thermostat'},
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e5300037", "type": "BLEES", "humanName": 'bwrc BLEE: center of student seating area'},
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e5900094", "type": "Blink", "humanName": 'bwrc Blink: rabaey conference room'}, 
            { "logname": "edu.berkeley.eecs.bwrc.device.c098e5900087", "type": "Blink", "humanName": 'bwrc Blink: broderson conference room'}, 


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
            { "logname": "edu.berkeley.eecs.swarmlab.device.c098e530007c", "type": "BLEES", "humanName": 'immersion BLEE: Chair 12'},

//added sept. 21: jacobs hall, TAMU
            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5700113", "type": "PowerBlade", "humanName": 'jacobs PowerBlade: 3D printer 10026 rm 110'},
//            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5700122", "type": "PowerBlade", "humanName": 'jacobs PowerBlade: 3D printer 10026B rm 110'},
            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5700123", "type": "PowerBlade", "humanName": 'jacobs PowerBlade: 3D printer 10047 rm 110'}, 
//            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5700125", "type": "PowerBlade", "humanName": 'jacobs PowerBlade: 3D printer 10047 rm 110'},           
            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5700126", "type": "PowerBlade", "humanName": 'jacobs PowerBlade: Laser 2 rm 110C'},
            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5700127", "type": "PowerBlade", "humanName": 'jacobs PowerBlade: 3D printer 10097 rm 110'},
            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5700129", "type": "PowerBlade", "humanName": 'jacobs PowerBlade: 3D printer 10213 rm 110'},
            { "logname": "edu.berkeley.eecs.jacobs.device.c098e57000e2", "type": "PowerBlade", "humanName": 'jacobs PowerBlade: Laser 1 rm 110C'},
            { "logname": "edu.berkeley.eecs.jacobs.device.c098e57000e5", "type": "PowerBlade", "humanName": 'jacobs PowerBlade: 3D printer 10035 rm 110'},
//            { "logname": "edu.berkeley.eecs.jacobs.device.c098e57000d3", "type": "PowerBlade", "humanName": 'jacobs PowerBlade: 3D printer 10098 rm 110'},
            { "logname": "edu.berkeley.eecs.jacobs.device.c098e57000d7", "type": "PowerBlade", "humanName": 'jacobs PowerBlade: 3D printer 10025 rm 110'},
            { "logname": "edu.berkeley.eecs.jacobs.device.c098e57000f7", "type": "PowerBlade", "humanName": 'jacobs PowerBlade: 3D printer 10038 rm 110'},
            { "logname": "edu.berkeley.eecs.jacobs.device.c098e57000db", "type": "PowerBlade", "humanName": 'jacobs PowerBlade: 3D printer 10094 rm 110'},
            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5700131", "type": "PowerBlade", "humanName": 'jacobs PowerBlade: 3D printer 10034 rm 110'},

//            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5300057", "type": "BLEES", "humanName": 'jacobs BLEE: rm 110 by thermostat'},
//            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5300055", "type": "BLEES", "humanName": 'jacobs BLEE: rm 110 check placement in room'},

            { "logname": "edu.berkeley.eecs.jacobs.device.c098e570008a", "type": "PowerBlade", "humanName": 'jacobs PowerBlade: Carbon 3D printer rm 332'},
            { "logname": "edu.berkeley.eecs.jacobs.device.c098e570010c", "type": "PowerBlade", "humanName": 'jacobs PowerBlade: Dim. 2 (Bonnie) rm 332'},
            { "logname": "edu.berkeley.eecs.jacobs.device.c098e570012e", "type": "PowerBlade", "humanName": 'jacobs PowerBlade: Dim. 1 (Clyde) rm 332'},

//            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5300055", "type": "BLEES", "humanName": 'jacobs BLEE: rm 120'},
            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5300050", "type": "BLEES", "humanName": 'jacobs BLEE: rm 332 Fortus 380 air exhaust vent'},
            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5300051", "type": "BLEES", "humanName": 'jacobs BLEE: rm 332 center of room'},

            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5900085", "type": "Blink", "humanName": 'jacobs Blink: rm 332'}, 
//            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5900094", "type": "Blink", "humanName": 'jacobs Blink: name unknown1'},
//            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5900087", "type": "Blink", "humanName": 'jacobs Blink: name unknown2'}, 

            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5900081", "type": "Blink", "humanName": 'jacobs Blink: rm 10C woodshop'}, 

            { "logname": "edu.berkeley.eecs.jacobs.device.c098e5300062", "type": "BLEES", "humanName": 'jacobs BLEE: rm 10C woodshop'},


            { "logname": "edu.tamu.esp.Setup1c.device.c098e530001d", "type": "BLEES", "humanName": 'TAMU BLEE'}, 
//            { "logname": "edu.tamu.esp.Setup1c.device.c098e590010", "type": "Blink", "humanName": 'TAMU Blink1'}, 
//            { "logname": "edu.tamu.esp.Setup1c.device.c098e530011", "type": "Blink", "humanName": 'TAMU Blink2'}, 
//            { "logname": "edu.tamu.esp.Setup1c.device.c098e530012", "type": "Blink", "humanName": 'TAMU Blink3'}, 
//            { "logname": "edu.tamu.esp.Setup1c.device.c098e5300ac", "type": "Blink", "humanName": 'TAMU Blink4'}, 

//            { "logname": "edu.tamu.esp.Setup1c.device.c098e5700028", "type": "PowerBlade", "humanName": 'TAMU PowerBlade: unknown1'}, 
            { "logname": "edu.tamu.esp.Setup1c.device.c098e57000c4", "type": "PowerBlade", "humanName": 'TAMU PowerBlade: unknown2'}, 
            { "logname": "edu.tamu.esp.Setup1c.device.c098e57000a8", "type": "PowerBlade", "humanName": 'TAMU PowerBlade: unknown3'}, 

//clark kerr sensors added 9/6
            { "logname": "edu.berkeley.eecs.clark-kerr.device.c098e5900098", "type": "Blink", "humanName": 'CKC Blink Krutch Auditorium entrance by stage'}, 
            { "logname": "edu.berkeley.eecs.clark-kerr.device.c098e5900083", "type": "Blink", "humanName": 'CKC Blink Krutch Auditorium rear wall of theater'}, 
            { "logname": "edu.berkeley.eecs.clark-kerr.device.c098e5900016", "type": "Blink", "humanName": 'CKC Blink Krutch Auditorium mid wall stage left'}, 
            { "logname": "edu.berkeley.eecs.clark-kerr.device.c098e590002d", "type": "Blink", "humanName": 'CKC Blink room 102 facing south door'}, 
            { "logname": "edu.berkeley.eecs.clark-kerr.device.c098e53000ae", "type": "BLEES", "humanName": 'CKC BLEES Krutch Auditorium entrance by stage'},
            { "logname": "edu.berkeley.eecs.clark-kerr.device.c098e5300060", "type": "BLEES", "humanName": 'CKC BLEES Krutch Auditorium rear wall of theater'},
            { "logname": "edu.berkeley.eecs.clark-kerr.device.c098e53000ad", "type": "BLEES", "humanName": 'CKC BLEES Krutch Auditorium mid wall stage left'}, 
            { "logname": "edu.berkeley.eecs.clark-kerr.device.c098e53000a6", "type": "BLEES", "humanName": 'CKC BLEES room 102 facing south door'},
            { "logname": "edu.berkeley.eecs.clark-kerr.device.c098e53000a7", "type": "BLEES", "humanName": 'CKC BLEES room 102 facing north door'},



],    };

deviceCategories = { //DEPENDENT
    "cats" : [
            { "category": "place", "id": 2},
            { "category": "type", "id": 1},
//            { "category": "room", "id": 3},
//            { "category": "university", "id": 4}
], };

place = {
    "offices" : [
         { "officename" : "bwrc" },
         { "officename" : "swarmlab" },
         { "officename" : "jacobs" },
         { "officename" : "tamu" },
         { "officename" : "clark-kerr" }
], };

typelist = {
    "types" : [
         { "type" : "BLEES" },
         { "type" : "Blink" },
         { "type" : "PowerBlade" },
], };

function selectSubcats(doc) {
    var subcatPickerString = "";
    var e = document.getElementById('category'); //dependent
    var selection = e.options[e.selectedIndex].value;
    if (selection == 1) {
    //sort by type
	 for (var i=0;i<typelist.types.length;i++) {
            var subcat = typelist.types[i];
            subcatPickerString += "<option value='" + subcat.type + "'>" + subcat.type + "</option>";
         }
    }
    if (selection == 2) {
    //sort by place (swarmlab, bwrc, ...)
         for (var i=0;i<place.offices.length;i++) {
            var subcat = place.offices[i];
            subcatPickerString += "<option value='" + subcat.officename + "'>" + subcat.officename + "</option>";
            }
    } 
    doc.innerHTML=subcatPickerString;
}

function selectLogs(doc) {
    var devicePickerString = ""; 
    var e = document.getElementById('subcat'); //dependent
    var f = document.getElementById('category');
    var cat = f.options[f.selectedIndex].value;

    for (var i=0; i<deviceData.logs.length; i++) {
        var log = deviceData.logs[i];
        if (cat == 1) { 
            if (log.type == e.options[e.selectedIndex].value) { //DEPENDENT
                devicePickerString += "<option value='" + log.logname + "'>" + log.humanName + "</option>"; 
            }
        }   
        if (cat == 2) {
            if (log.logname.includes(e.options[e.selectedIndex].value)) {
                devicePickerString += "<option value='" + log.logname + "'>" + log.humanName + "</option>"; 
            }
        }
    }
    doc.innerHTML = devicePickerString;
} 

function makeForm() {
    // Code for picking up a device
    var catPickerString = ""; //DEPENDENT
    for (var j=0; j<deviceCategories.cats.length; j++) {
        var cat = deviceCategories.cats[j];
        catPickerString += "<option value='" + cat.id + "'>" + cat.category + "</option>";
    }
    document.getElementById('category').innerHTML = catPickerString;
    selectSubcats(document.getElementById('subcat'));
    selectLogs(document.getElementById('logname'));
    document.getElementById('category').onchange=function(){
        selectSubcats(document.getElementById('subcat'));
	selectLogs(document.getElementById('logname'));
    }
    document.getElementById('subcat').onchange=function(){
        selectLogs(document.getElementById('logname'));
    }
	
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

    if (response.isError()==true) {
        document.getElementById('request_status').innerHTML = response.getMessage();
        return;
    }

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

