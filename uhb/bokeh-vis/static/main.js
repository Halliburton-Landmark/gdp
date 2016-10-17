// All javascript functions go here...
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
    urlString = "http://localhost:5006/main?log=edu.berkeley.eecs.bwrc.device.c098e5300009&log=edu.berkeley.eecs.swarmlab.device.c098e5300003&log=edu.berkeley.eecs.swarmlab.device.c098e530000a&start=1473225641.397882&end=1475817641.397882&height=200&width=800&plot_0_title=Light&plot_0_keys=light_lux&plot_1_title=Temperature&plot_1_keys=temperature_celcius&plot_2_title=Humidity&plot_2_keys=humidity_percent";
    document.getElementById('dashboard_div').innerHTML = "<iframe src=\"" + urlString + "\"></iframe>"

}

