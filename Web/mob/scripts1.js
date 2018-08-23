/* ver 0.895 beta */
var urlcontrol = 'http://77.50.254.24:25402'; // адрес и порт контроллера, если адрес сервера отличен от адреса контроллера (не рекомендуется)
//var urlcontrol = ''; //  если адрес сервера совпадает с адресом контроллера (рекомендуется)
var urltimeout = 2800; // таймаут ожидание ответа от контроллера. Чем хуже интертнет, тем выше значения. Но не более времени обновления параметров
var urlupdate = 5000; // время обновления параметров в в миллисекундах


function setParam(paramid, resultid)
{
var elid = paramid.replace(/\(/g,"-").replace(/\)/g,"").toLowerCase().replace(/get_/g,"get_");   //console.log("elid:"+elid+" paramid:"+paramid);
var rec = new RegExp('et_listChart'); // ãðàôèê
var rel = new RegExp('et_sensorListIP'); 
var res = new RegExp('et_sensorListIP|et_freon|et_rule|et_listProfile|et_testMode|et_FC|et_EEV|et_modeHP');
//var ren = new RegExp('et_ON|et_OFF');
var ret = new RegExp('[(]SCHEDULER[)]');
var rer = new RegExp('BIG_NARMON|USE_THINGSPEAK|ADD_BOILER|USE_NARMON|USE_MQTT|BIG_MQTT|SDM_MQTT|FC_MQTT|COP_MQTT|NO_PING|set_SDM|ENABLE_PROFILE|ON_OFF|get_paramFC[(]ANALOG[)]|sensorUseIP|sensorRuleIP|NO_ACK|MESS_RESET|MESS_WARNING|MESS_SD|MESS_TEMP|MESS_LIFE|MESS_ERROR|[(]MAIL[)]|MAIL_AUTH|MAIL_INFO|[(]SMS[)]|et_Relay|UPDATE_I2C|et_optionHP[(]SAVE_ON|Boiler[(]BOILER_ON|Boiler[(]SCHEDULER_ON|Boiler[(]ADD_HEATING|Boiler[(]SALLMONELA|et_Network[(]DHCP|INIT_W5200|et_Network[(]PASSWORD|RESET_HEAT|CIRCULATION|et_optionHP[(]SD_CARD|[(]WEATHER[)]|et_optionHP[(]BEEP|RTV_OFF|et_optionHP[(]NEXTION|et_optionHP[(]EEV_CLOSE|et_optionHP[(]EEV_START|get_datetime[(]UPDATE');
if (rer.test(paramid)) { swichid = document.getElementById(elid); if (swichid.checked) { elval =1; } else { elval = 0; } }
//else if (ren.test(paramid)) { swichid = document.getElementById(elid); if (swichid.checked) { elval =""; } else { elval = ""; } }
else if (ret.test(paramid)) {//console.log("sended sheduler");
              var divscheduler = document.getElementById("get_boiler-scheduler");           
              var inputs = divscheduler.getElementsByTagName("input");
              var elval = ""; //console.log("lenght:"+inputs.length);
              for (var i = 1; i < inputs.length; i++){
                if (inputs[i-1].checked){ elval += 1; } else { elval += 0; }
                if ( i % 24 == 0) {elval +="/";}
              }
  }
  else { //  console.log("elid:"+elid+" paramid:"+paramid);
  var elval = document.getElementById(elid).value; //console.log("!!!!!!!elid:"+elid+" !!!!!!!value:"+elval);
  }
if (res.test(paramid)) {var elsend = paramid.replace(/get_/g,"set_").replace(/\)/g,"") + "(" + elval +")";}
else if(rec.test(paramid)) { var elsend = paramid.replace(/get_listChart/g,"get_Chart("+elval+")"); }
else {var elsend = paramid.replace(/get_/g,"set_").replace(/\)/g,"") + "=" + elval +")";}
if(resultid == "" || resultid == null )  {
  var valueid = paramid.replace(/set_/g,"get_").replace(/\(/g,"-").replace(/\)/g,"").toLowerCase();
} else {
  var valueid = resultid;
}
if(rel.test(paramid)) { elsend=elsend.replace(/\(/g,"=").replace(/\-/g,"(");}

if(!rec.test(paramid)) { document.getElementById(valueid).value = ""; document.getElementById(valueid).placeholder  = "";
//if (elsend == "set_ON=)") {elsend="set_ON";}
//	else if	(elsend == "set_OFF=)") {elsend="set_OFF";}
}

loadParam(encodeURIComponent(elsend), true, valueid);
}


function loadParam(paramid, noretry, resultdiv)
{
var check_ready = 1;
var queue = 0;
var req_stek = new Array();
  if (queue == 0)
  {
  req_stek.push(paramid);
  }
  else if (queue == 1)
  {
  req_stek.unshift(paramid);
  queue = 0;
  }
  
  if (check_ready == 1)
  { unique(req_stek);
  var oneparamid = req_stek.shift();
  check_ready = 0;
    
var request = new XMLHttpRequest(); 
var reqstr = oneparamid.replace(/,/g,'&'); 
request.open("GET", urlcontrol + "/&" + reqstr + "&&", true); //console.log(reqstr);
request.timeout = urltimeout; 
request.send(null);
request.onreadystatechange = function()
  {
  if (this.readyState != 4) return;
    if (request.status == 200)
    { 
      if (request.responseText != null)
      {
		var arr = request.responseText.replace(/^&+/, '').replace(/&&*$/, '').split('\x7f');
        if (arr != null && arr != 0  ) {
          check_ready = 1; // ответ получен, можно слать следующий запрос.
          if (req_stek.length != 0) // если массив запросов не пуст - заправшиваем следующие значения.
          {
            queue = 1;
            setTimeout(function() { loadParam(req_stek.shift()); }, 10); // запрашиваем следующую порцию.
          }         
        for (var i = 0; i < arr.length; i++) { //console.log(arr.length);
          if (arr[i] != null && arr[i] != 0) {
            var reerr = new RegExp('^E');
            var rec = new RegExp('^CONST|get_infoFC|get_sysInfo|get_socketInfo|get_status');
            var rei = new RegExp('listFlow|listTemp|listInput|listRelay|sensorIP|get_numberIP|NUM_PROFILE');
            var reo = new RegExp('^scan_');
            var rer = new RegExp('^get_Relay|ENABLE_PROFILE|BIG_NARMON|ADD_BOILER|USE_NARMON|USE_THINGSPEAK|USE_MQTT|BIG_MQTT|SDM_MQTT|FC_MQTT|COP_MQTT|NO_PING|ON_OFF|get_paramFC[(]ANALOG[)]|sensorRuleIP|sensorUseIP|NO_ACK|MESS_RESET|MESS_WARNING|MESS_SD|MESS_TEMP|MESS_LIFE|MESS_ERROR|[(]MAIL[)]|MAIL_AUTH|MAIL_INFO|[(]SMS[)]|UPDATE_I2C|Boiler[(]BOILER_ON|Boiler[(]SCHEDULER_ON|Boiler[(]ADD_HEATING|CIRCULATION|RESET_HEAT|Boiler[(]SALLMONELA|et_Network[(]DHCP|INIT_W5200|et_Network[(]PASSWORD|et_optionHP[(]ADD_HEA|et_optionHP[(]BEEP|RTV_OFF|get_optionHP[(]SAVE_ON|et_optionHP[(]NEXTION|et_optionHP[(]SD_CARD|[(]WEATHER[)]|et_optionHP[(]EEV_CLOSE|et_optionHP[(]EEV_START|get_datetime[(]UPDATE');
            var rep = new RegExp('^get_present');
            var ret = new RegExp('[(]SCHEDULER[)]');
            var res = new RegExp('PING_TIME|et_listFlow|et_listPress|et_sensorListIP|et_freon|et_rule|et_testMode|get_listProfile|et_listChart|HP[(]RULE|HP[(]TARGET|SOCKET|RES_W5200|et_modeHP|TIME_CHART|SMS_SERVICE|et_optionHP[(]ADD_HEAT');
            var rev = new RegExp(/\([a-z0-9_]+\)/i);
            var reg = new RegExp('^get_Chart');
            var remintemp = new RegExp('^get_mintemp');
            var remaxtemp = new RegExp('^get_maxtemp');
            if (rec.test(arr[i])) { var type = "const"; }
            else if (rei.test(arr[i])) { var type = "sensorip";  }  // 
            else if (res.test(arr[i])) { var type = "select";  }  // значения
            else if (reg.test(arr[i])) { var type = "chart";  }  // график
            else if (ret.test(arr[i])) { var type = "scheduler";  }  // расписание
            else if (reo.test(arr[i])) { var type = "scan";  } // ответ на сканирование
            else if (rep.test(arr[i])) { var type = "present";  } // наличие датчика в конфигурации
            else if (rer.test(arr[i])) { var type = "checkbox";  }  // чекбокс
            else if (rev.test(arr[i])) { var type = "values";  }  // значения
            else { var type = "str"; }
            values = arr[i].split('=');   //console.log(type + ":^^^^^^ " + values[0] + "############"  + values[1] );
			
		if(type == 'scheduler') {
              var divscheduler = document.getElementById("get_boiler-scheduler");           
              var inputs = divscheduler.getElementsByTagName("input");
              cont1 = values[1].replace(/\//g,""); 
              for (var i = 0; i < inputs.length; i++){
                if (cont1.charAt(i) == 1){ inputs[i].checked = true; } else { inputs[i].checked = false; }
              }
            } else if(type == 'chart') {
              if (values[0] != null && values[0] != 0 && values[1] != null && values[1] != 0) { //console.log(type + " values 0= " + values[0] + " values1="  + values[1] );
                title = values[0].replace(/get_Chart\(_/g,"").replace(/\)[0-9]?/g,""); //console.log(" titul= " + title );
                var yizm = ''; var ytooltip = ''; var timeval = ''; var height = 250; var visible = false; 
                timeval = window.time_chart;
                timeval = Number(timeval.replace(/\D+/g,""));
                var today = new Date();
                var regexpt = /^(T|O|d)/g;var regexpp = /^PE/g;var regexpe = /^pos/g;var regexpr = /^R/g;var regexpw = /^Pow|pow/g;
                  if(regexpt.test(title)) { yizm = "Температура, °C"; ytooltip = " °C"; }
                  if(regexpp.test(title)) { yizm = "Давление, BAR"; ytooltip = " BAR"; }
                  if(regexpr.test(title)) { yizm = "Состояние реле"; ytooltip = ""; }
                  if(regexpe.test(title)) { yizm = "Позиция, шаги"; ytooltip = " шаг";  }
				  if(regexpw.test(title)) { yizm = "Мощьность, кВт"; ytooltip = " kW";  }
                  data = values[1].split(';');  //console.log("data!!!!!:"+data);
				  //if (data != null && data != 0) { data = "0";} console.log("data!!!!!: break= "+data);
                  
                  var dataSeries1 = [];
                  //dataSeries1.pointInterval = timeval; 
                  if (title == 'posEEV')  { var poseev = []; }
                   //dataSeries.pointStart = parseInt(start); // its important to parseInt !!!!
                    for (var i = 0; i < data.length - 1; i++) {
                      //today.setSeconds(today.getSeconds() + 10);  console.log(today.toLocaleTimeString());
						if (title == 'posEEV')    { poseev.push([i, Number(data[i])]); }
						else {
								dataSeries1.push([i, Number(data[i])]); //data: [{ x: 1880, y: -0.4 }, { x: 2014, y: 0.52 }]
							 }	  
                      }          
                  
                  if (title == 'posEEV')  { window.poseev = poseev; dataSeries1 = poseev;}
                  if (title == 'OVERHEAT')  { dataSeries2 = window.poseev ; visible = true; }  else { var dataSeries2 = [];}
                  //if (title == 'posEEV') {break;}
                  
                  $('#'+resultdiv).highcharts({
                    title: { text: title ,
					style: {
            			color: '#ffffff',
        						}
							},
                    chart: { backgroundColor: 'rgba(0, 0, 0, 0.2)', borderWidth:'1', borderColor: '#ffcccc90', borderRadius: 3, type: 'line', height:height, resetZoomButton: {position: {align: undefined, verticalAlign: "top", x: 20, y: -40}, relativeTo: "plot"} },
                    lang: { contextButtonTitle: "Меню графика", decimalPoint: ".", downloadJPEG: "Скачать JPEG картинку", downloadPDF: "Скачать PDF документ", downloadPNG: "Скачать PNG картинку", downloadSVG: "Скачать SVG векторную картинку", drillUpText: "Вернуться к {series.name}", loading: "Загрузка...", noData: "Нет информации для отображения", numericSymbolMagnitude: 1000, numericSymbols: [ "k" , "M" , "G" , "T" , "P" , "E"], printChart: "Распечатать график", resetZoom: "Сброс увеличения", resetZoomTitle: "Сброс увеличения к 1:1" },
                    xAxis: {maxPadding: 0.05, lineColor: '#ffffff', title: { text: "Время, шаг: x" + window.time_chart }, color: '#ffffff' },
                    /*yAxis: [{ title: { text: yizm },labels: {align: 'left', x: 0, y: 0, format: '{value:.,0f}' }, plotLines: [{  value: 0,  width: 1,  color: '#808080'  }] },
							{ title: { text: 'Положение ЭРВ(шаги)' },labels: {align: 'right', x: 0, y: 0, format: '{value:.,0f}' }, plotLines: [{  value: 0,  width: 1,  color: '#808080'  }] },
							opposite: true],*/
					yAxis: [{ // Primary yAxis
						minRange: 1,	
						gridLineColor: '#ffffff',
						maxPadding: 0.05,
						allowDecimals: false,
        				minorTickLength: 0,
						minorTickColor: '#ffffff',
						gridLineWidth: 0.2,
						tickAmount: 3,
						labels: {
									format: '{value}',
									style: { color: '#ffffff' }
								},
				        title: {
									text: yizm,
									style: { color: '#ffffff' }
								}
					        }, { // Secondary yAxis
						allowDecimals: false,
						showEmpty: false,
						visible: visible,
						title: {
							text: 'Положение ЭРВ',
							style: { color: Highcharts.getOptions().colors[5] }
								},
						labels: {
							format: '{value} шагов',
							style: { color: Highcharts.getOptions().colors[6] }
							},
					opposite: true
					}],
                    tooltip: {valueSuffix: '' },
                    legend: { layout: 'vertical',  align: 'right', verticalAlign: 'middle', borderWidth: 0 },
                    //plotOptions: { series: { dataGrouping: { enabled: false } } },
                    plotOptions: { series: { label: { connectorAllowed: false }, pointStart: 0, color: '#C1272D', threshold:0, negativeColor: true, negativeColor: '#0088FF', lineWidth: 5 } },
                    navigation: { buttonOptions: {enabled: false} },
					
					series: [
						{ yAxis: 0, name: title, tooltip: { valueDecimals: 2 }, states: { hover: { enabled: false }}, showInLegend: false, turboThreshold: 0, data:  dataSeries1, dashStyle: "Solid" },
					    { yAxis: 1, name: 'Положение ЭРВ', tooltip: { valueDecimals: 2 }, states: { hover: { enabled: false }}, showInLegend: false, turboThreshold: 0, data:  dataSeries2, dashStyle: "Solid" }
						]
						
                  });
              }  
            } else if (type == 'scan') {
                if (values[0] != null && values[0] != 0 && values[1] != null && values[1] != 0) {
                  cont1 = values[1].replace(/\:/g,"</td><td>"); console.log(cont1);
                  cont2 = cont1.replace(/(\;)/g,"</td></tr><tr><td>"); console.log(cont2);
                  var content = "<tr><td>" + cont2 + "</td></tr>";
                  document.getElementById(values[0].toLowerCase()).innerHTML = content;
                  cont3 = values[1].replace(/[0-9]:.{1,10}:[-0-9\.]{1,5}:/g,""); cont3 = cont3.replace(/;$/g,""); console.log(cont3);
                  var cont4 = cont3.split(';');  console.log(cont3);         
                  for (var j=0;j<50;j++) {
                    if(document.getElementsByTagName('select')[j]) {
                    document.getElementsByTagName('select')[j].options.length = 0
                    optdef = new Option("---", "", true, true);
                    document.getElementsByTagName('select')[j].add(optdef,null);
                    optres = new Option("reset", 0, false, false);
                    document.getElementsByTagName('select')[j].add(optres,null);
                    for (var i=0,len=cont4.length;i<len;i++) {
                      opt = new Option(i+1, i+1, false, false);
                      document.getElementsByTagName('select')[j].add(opt,null);
                    }
                  }
                  }
                }
            } else if (type == 'select') {   //console.log(type + ": " + values[0] + "<-->"  + values[1] );
                if (values[0] != null && values[0] != 0 && values[1] != null && values[1] != 0) {
                 var idsel = values[0].toLowerCase().replace(/set_/g,"get_").replace(/\([0-9]\)/g,"").replace(/\(/g,"-").replace(/\)/g,"");
                 if(idsel == 'get_sensorlistip') idsel = values[0].toLowerCase().replace(/set_/g,"get_").replace(/\(/g,"-").replace(/\)/g,"");
                 //console.log(type + ": " + idsel + "<-->"  + values[1] );
                if (idsel=="get_testmode") { 
				var element2=document.getElementById("get_testmode2");
                  if(element2) {
                    var cont1 = values[1].split(';');
                    for (var n=0,len=cont1.length;n<len;n++) { 
                        cont2 = cont1[n].split(':');
                        if (cont2[1] == 1)  {
                          if (cont2[0] != "NORMAL") {
							  document.getElementById("get_testmode2").className = "red";}
                          else {
						document.getElementById("get_testmode2").className = "";}
						document.getElementById("get_testmode2").innerHTML = cont2[0];
						}
                      }
					  }
					  }
                else if (idsel=="get_ruleeev") {
                  document.getElementById("get_freoneev").disabled = false;
                  document.getElementById("get_parameev-m_step").disabled = false;
                  document.getElementById("get_parameev-m_step2").disabled = false;
                  document.getElementById("get_parameev-correction").disabled = false;
                  document.getElementById("get_parameev-correction2").disabled = false;
                  document.getElementById("get_parameev-t_owerheat").disabled = false;
                  document.getElementById("get_parameev-t_owerheat2").disabled = false;
                  document.getElementById("get_parameev-time_in").disabled = false;
                  document.getElementById("get_parameev-time_in2").disabled = false;
                  document.getElementById("get_parameev-k_pro").disabled = false;
                  document.getElementById("get_parameev-k_pro2").disabled = false;
                  document.getElementById("get_parameev-k_in").disabled = false;
                  document.getElementById("get_parameev-k_in2").disabled = false;
                  document.getElementById("get_parameev-k_dif").disabled = false;
                  document.getElementById("get_parameev-k_dif2").disabled = false;
                }
                else if (idsel=="get_message-smtp_server") {
                    loadParam("get_Message(SMTP_IP),get_Message(SMS_IP)");
                }   
                else if (idsel=="get_message-sms_service") {
                    loadParam("get_Message(SMS_NAMEP1),get_Message(SMS_NAMEP2),get_Message(SMS_IP)");
                }
                else if (idsel=="get_message-sms_service") {
                    loadParam("get_Message(SMS_NAMEP1),get_Message(SMS_NAMEP2),get_Message(SMS_IP)");
                }
                else if (idsel=="get_listpress") {
                  content=""; content2=""; upsens = ""; loadsens=""; 
                  var count = values[1].split(';'); // console.log("get_listpress:" + count);
                    for (var i=0;i<count.length-1;i++) {
                      input = count[i].toLowerCase(); 
                      loadsens = loadsens + "get_zeroPress("+count[i]+"),get_transPress("+count[i]+"),get_maxPress("+count[i]+"),get_minPress("+count[i]+"),get_pinPress("+count[i]+"),get_notePress("+count[i]+"),get_testPress("+count[i]+"),";
                      upsens = upsens + "get_Press("+count[i]+"),get_adcPress("+count[i]+"),get_errcodePress("+count[i]+"),";

content = content + '<table><tr id="get_presentpress-'+input+'">';
content = content + '<tr class="sensor_a"><td>Имя датчика</td><td>'+count[i]+'</td>';
content = content + '<tr class="sensor_p"><td>Описание датчика</td><td id="get_notepress-'+input+'"></td></tr>';
content = content + '<tr class="sensor_p"><td>Значение</td><td id="get_press-'+input+'">-</td></tr>';
content = content + '<tr class="sensor_p"><td>Минимум</td><td id="get_minpress-'+input+'">-</td></tr>';
content = content + '<tr class="sensor_p"><td>Максимум</td><td id="get_maxpress-'+input+'">-</td></tr>';
content = content + '<tr class="sensor_p"><td> <input class="buttont" type="submit" value="«0» датчика"  onclick="setParam(\'get_zeroPress('+count[i]+')\');"></td><td><input id="get_zeropress-'+input+'" type="number" min="0" max="2048" step="1" value=""> </td></tr>';
content = content + '<tr class="sensor_p"><td> <input class="buttont" type="submit" value="Коэффициент"  onclick="setParam(\'get_transPress('+count[i]+')\');"></td><td><input id="get_transpress-'+input+'" type="number" min="0" max="4" step="0.001" value=""> </td></tr>';
content = content + '<tr class="sensor_p"><td> <input class="buttont" type="submit" value="Тест датчика"  onclick="setParam(\'get_testPress('+count[i]+')\');"></td><td><input id="get_testpress-'+input+'" type="number" min="-1" max="50" step="0.01" value=""> </td></tr>';    
content = content + '<tr class="sensor_p"><td>Pin</td><td id="get_pinpress-'+input+'">-</td></tr>';
content = content + '<tr class="sensor_p"><td>ADC</td><td id="get_adcpress-'+input+'">-</td></tr>';
content = content + '<tr class="sensor_p"><td>Код ошибки</td><td id="get_errcodepress-'+input+'">-</td>';
content = content + '</tr></table><p></p>';
                    
                    }
                    document.getElementById(idsel+"2").innerHTML = content; //console.log("get_numberIP-content " + content);
                    updateParam(upsens); //console.log("upsens " + upsens);
                    loadParam(loadsens); //console.log("loadsens: " + loadsens);
                    values[1] = "Выбрать датчик;" + values[1];
                

}                                
                  var element=document.getElementById(idsel);
                  if(element) {document.getElementById(idsel).innerHTML="";}
                  var element3 = document.getElementById(idsel);
                  //console.log("idsel: "+idsel); 
                  if ( element3 && element3.tagName != "SPAN") {
                    var cont1 = values[1].split(';');
                    for (var k=0,len=cont1.length-1;k<len;k++) {
                    cont2 = cont1[k].split(':'); 
                    //console.log("cont1: "+cont1+" cont2:"+cont2); 
                    if (cont2[1] == 1)  {   
                      selected = true;
                      if (idsel=="get_optionhp-time_chart") {
                        var time_chart = cont2[0];
                        window.time_chart = time_chart;
                        }                     
                      if (idsel=="get_ruleeev") {
                      if (k==0 || k==1) {
                      document.getElementById("get_freoneev").disabled = true;
                      document.getElementById("get_parameev-m_step").disabled = true;
                      document.getElementById("get_parameev-m_step2").disabled = true;                      
                      }
                      else if (k==2 || k==3){
                      document.getElementById("get_parameev-m_step").disabled = true;
                      document.getElementById("get_parameev-m_step2").disabled = true;
                      }
                      else if (k==4){
                      document.getElementById("get_parameev-t_owerheat").disabled = true;
                      document.getElementById("get_parameev-t_owerheat2").disabled = true;
                      document.getElementById("get_parameev-k_pro").disabled = true;
                      document.getElementById("get_parameev-k_pro2").disabled = true;
                      document.getElementById("get_parameev-k_in").disabled = true;
                      document.getElementById("get_parameev-k_in2").disabled = true;
                      document.getElementById("get_parameev-k_dif").disabled = true;
                      document.getElementById("get_parameev-k_dif2").disabled = true;
                      document.getElementById("get_freoneev").disabled = true;
                      document.getElementById("get_parameev-correction").disabled = true;
                      document.getElementById("get_parameev-correction2").disabled = true;
                      document.getElementById("get_parameev-m_step").disabled = true;
                      document.getElementById("get_parameev-m_step2").disabled = true;
                      }
                      else if(k==5){
                      document.getElementById("get_parameev-t_owerheat").disabled = true;
                      document.getElementById("get_parameev-t_owerheat2").disabled = true;
                      document.getElementById("get_parameev-k_pro").disabled = true;
                      document.getElementById("get_parameev-k_pro2").disabled = true;
                      document.getElementById("get_parameev-k_in").disabled = true;
                      document.getElementById("get_parameev-k_in2").disabled = true;
                      document.getElementById("get_parameev-k_dif").disabled = true;
                      document.getElementById("get_parameev-k_dif2").disabled = true;
                      document.getElementById("get_freoneev").disabled = true;
                      document.getElementById("get_parameev-correction").disabled = true;
                      document.getElementById("get_parameev-correction2").disabled = true;
                      }
                      } else if (idsel=="get_paramcoolhp-rule") {
                        document.getElementById("get_paramcoolhp-target").disabled = false;
                        document.getElementById("get_paramcoolhp-dtemp").disabled = false;
                        document.getElementById("get_paramcoolhp-hp_pro").disabled = false;
                        document.getElementById("get_paramcoolhp-hp_in").disabled = false;
                        document.getElementById("get_paramcoolhp-hp_dif").disabled = false;
                        document.getElementById("get_paramcoolhp-temp_pid").disabled = false;
                        document.getElementById("get_paramcoolhp-weather").disabled = false;
                        document.getElementById("get_paramcoolhp-k_weather").disabled = false;
                        document.getElementById("get_paramcoolhp-hp_time").disabled = false;
                            if (k==2) { document.getElementById("get_paramcoolhp-target").disabled = true;
                         } else if (k==1){ document.getElementById("get_paramcoolhp-dtemp").disabled = true; 
                         } else if (k==0){ document.getElementById("get_paramcoolhp-hp_time").disabled = true;
                           document.getElementById("get_paramcoolhp-hp_pro").disabled = true;
                           document.getElementById("get_paramcoolhp-hp_in").disabled = true;
                           document.getElementById("get_paramcoolhp-hp_dif").disabled = true;
                           document.getElementById("get_paramcoolhp-temp_pid").disabled = true;
                           document.getElementById("get_paramcoolhp-weather").disabled = true;
                           document.getElementById("get_paramcoolhp-k_weather").disabled = true;
                            }
                      } else if (idsel=="get_paramheathp-rule") {
                        document.getElementById("get_paramheathp-target").disabled = false;
                        //document.getElementById("get_paramheathp-dtemp").disabled = false;
                        document.getElementById("get_paramheathp-hp_pro").disabled = false;
                        document.getElementById("get_paramheathp-hp_in").disabled = false;
                        document.getElementById("get_paramheathp-hp_dif").disabled = false;
                        document.getElementById("get_paramheathp-temp_pid").disabled = false;
                        document.getElementById("get_paramheathp-weather").disabled = false;
                        document.getElementById("get_paramheathp-k_weather").disabled = false;
                        document.getElementById("get_paramheathp-hp_time").disabled = false;
                            if (k==2) { document.getElementById("get_paramheathp-target").disabled = true;
                         } else if (k==1){ //document.getElementById("get_paramheathp-dtemp").disabled = true;
                         } else if (k==0){ document.getElementById("get_paramheathp-hp_time").disabled = true;
                           document.getElementById("get_paramheathp-hp_pro").disabled = true;
                           document.getElementById("get_paramheathp-hp_in").disabled = true;
                           document.getElementById("get_paramheathp-hp_dif").disabled = true;
                           document.getElementById("get_paramheathp-temp_pid").disabled = true;
                           document.getElementById("get_paramheathp-weather").disabled = true;
                           document.getElementById("get_paramheathp-k_weather").disabled = true;
                            }
                      } else if (idsel=="get_paramcoolhp-target") {
                        if (k==0) { document.getElementById("get_paramcoolhp-temp2").disabled = true; document.getElementById("get_paramcoolhp-temp1").disabled = false;
						document.getElementById('paramheathp_3').innerHTML = '<input id="get_paramcoolhp-temp1" type="number" min="0" max="30" step="0.1">' ;
						document.getElementById('paramheathp_4').innerHTML = '<input class="button2 small" id="get_paramcoolhp-temp12" type="submit" value="Установить температуру в доме"  onclick="setParam(\'get_paramCoolHP(TEMP1)\');">';
                        
						} else if (k==1) { document.getElementById("get_paramcoolhp-temp2").disabled = false; document.getElementById("get_paramcoolhp-temp1").disabled = true;
						document.getElementById('paramheathp_3').innerHTML = '<input id="get_paramcoolhp-temp2" type="number" min="10" max="50" step="0.1">' ;
						document.getElementById('paramheathp_4').innerHTML = '<input class="button2 small" id="get_paramcoolhp-temp22" type="submit" value="Установить температуру обратки"  onclick="setParam(\'get_paramCoolHP(TEMP2)\');">';
                        }
                      } else if (idsel=="get_paramheathp-target") {
                        if (k==0) { document.getElementById("get_paramheathp-temp2").disabled = true; document.getElementById("get_paramheathp-temp1").disabled = false;
						document.getElementById('paramheathp_1').innerHTML = '<input id="get_paramheathp-temp1" type="number" min="5" max="30" step="0.1">' ;
						document.getElementById('paramheathp_2').innerHTML = '<input class="button2 small" id="get_paramheathp-temp12" type="submit" value="Установить температуру в доме"  onclick="setParam(\'get_paramHeatHP(TEMP1)\');">';
                        
                        } else if (k==1) { document.getElementById("get_paramheathp-temp2").disabled = false; document.getElementById("get_paramheathp-temp1").disabled = true;
						document.getElementById('paramheathp_1').innerHTML = '<input id="get_paramheathp-temp2" type="number" min="5" max="50" step="0.1">' ;
						document.getElementById('paramheathp_2').innerHTML = '<input class="button2 small" id="get_paramheathp-temp22" type="submit" value="Установить температуру обратки"  onclick="setParam(\'get_paramHeatHP(TEMP2)\');">';
                        }
						
				
                      } 
					  		
                      
                    } else { selected = false; }
                    if(idsel=="get_listchart") {
                        for (var j=0;j<6;j++) {
                        opt = new Option(cont2[0], "_"+cont2[0], false, selected ); // console.log("j:"+j+" cont2[0]:"+cont2[0]+" idse:"+idsel+" selected:"+selected);
                        document.getElementsByTagName('select')[j].add(opt,null);
                        }
                      } else {
                        var opt = new Option(cont2[0], k, false, selected ); // console.log("cont2[0]:"+cont2[0]+" idse:"+idsel+" selected:"+selected);
                        if (cont2[2] == 0) opt.disabled = true;
                        var element = document.getElementById(idsel);
                        if(element) element.add(opt,null); 
                      }
                    }
                  }
                }   
            } else if (type == 'const') {
                if (values[0] != null && values[0] != 0 && values[1] != null && values[1] != 0) {
                  if(values[0]=='get_status') {
					cont1 = values[1].replace(/\|/g,"</div><div>");
                    var content = "<div>" + cont1 + "</div>"; 
                  } else {
                     cont1 = values[1].replace(/\|/g,"</td><td>"); //console.log("&&&&&&&&&&&&&&&get_numberIP:"+cont1);
                     cont2 = cont1.replace(/(\;)/g,"</td></tr><tr class=\"const\"><td>");
                     var content = "<tr class=\"const\"><td>" + cont2 + "</td></tr>"; //console.log("******************get_numberIP:"+content);
                  }
                  document.getElementById(values[0].toLowerCase()).innerHTML = content;   //console.log("******************get_numberIP:"+content);
                }
            } else if (type == 'sensorip') { 
              if (values[0] != null && values[0] != 0 && values[1] != null && values[1] != 0) {
                cont2 = values[0].replace(/\(/g,"-").replace(/\)/g,"").toLowerCase(); console.log("sensorip??????????????????????????????????????: "+cont2);
                if (values[0]=='get_numberIP') { console.log("get_numberIP!!");
                  content=""; content2=""; upsens=""; loadsens=""; count = Number(values[1]); console.log("get_numberIP:"+count);
                    for (var i=1;i<count+1;i++) {
                      upsens = upsens + "get_sensorIP("+i+"),get_sensorParamIP("+i+":SENSOR_TEMP),";
                      loadsens = loadsens + "get_sensorListIP("+i+"),get_sensorRuleIP("+i+"),get_sensorUseIP("+i+"),get_sensorParamIP("+i+":SENSOR_TEMP),get_sensorParamIP("+i+":STIME),get_sensorParamIP("+i+":SENSOR_IP),get_sensorParamIP("+i+":RSSI),get_sensorParamIP("+i+":VCC),get_sensorParamIP("+i+":SENSOR_COUNT),";
                      content_id = 'get_sensor_ip-'+i+''; console.log("content_id: "+content_id);
					  id_ip1 = ""; id_ip1 = id_ip1 + 'get_sensorparamip-'+i+':sensor_temp';
					  id_ip2 = ""; id_ip2 = id_ip2 + 'get_sensorparamip-'+i+':stime';
					  id_ip3 = ""; id_ip3 = id_ip3 + 'get_sensorparamip-'+i+':sensor_ip';
					  id_ip4 = ""; id_ip4 = id_ip4 + 'get_sensorparamip-'+i+':rssi';
					  id_ip5 = ""; id_ip5 = id_ip5 + 'get_sensorparamip-'+i+':vcc';
					  id_ip6 = ""; id_ip6 = id_ip6 + 'get_sensorparamip-'+i+':sensor_count';
					//  id_ip7 = ""; id_ip7 = id_ip7 + 'get_sensoruseip-'+i+'2';
					 // id_ip8 = ""; id_ip8 = id_ip8 + 'get_sensorruleip-'+i+'2';
					  content = "";
					  content = content + '<table><thead><tr class="tempt"><th>';
					  content = content + '<img  width="30" height="30"  src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB4AAAAeCAYAAAA7MK6iAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAA7pJREFUeNqsV0tME1EUvQPIr3wq1oCgghQRE4TGaFgoWCPCRg0hRtEonw0bTWxCdKMRNLDRmGDUDRsomogaCEY3IIkV4oJotCUk/kBB5acFCgWhVdF3n53J6+u0nYI3uZnpzJt77jn3vk8FUGhb7n5KJpci4jriKdzrIeJm4u3vjm0aVhJPUAhYQ7xcYY5NON5fAoIf0GoXqJv9ss9SRwuJjqEuYwh+KWBgAtrIslwc/Qpz79/Aj+GP8MfpdA8SGgqRyakQlb4VwhPXu7En4BWKgVnQJYcDrM+6YIEAKrEIkoBmTz4EhYX5BBd8yeuc/A5WUxf8nLJCILYqTgMafT6ErlnrVXZBppGGRKbjj9vcQHOztZDnctYsg6PQYxmER8/73cATDhSzzFPYhhO8SWw1PYH5D2/p8xMFO+BCWSEkJ8T9AxoYgZm5Bem7PF0avdrIs5ut3dRn5hdBtTmDMN8vK7nAgMbit6LEY20tkKVNhPuXKygggmFAZIVBeTu4KxNOF+fSJIbHp+DIxUboI0qsKy5hJVcT8BkeuMw1Bylb7dIs9DZUURZnb7XDnc6XdFysKhyy0pIkubuJxAj0eWJaKgcmi5ZTeQ0mY+JZ1uUE2EinIZO0Tpo6YyNgU4VArbFDkg0DIqNDu7fJNlS3eQBqmztprTOO10LDuRIpFodh5Bk/JRc9NtWX5gZpJDK8eqoIThbupOxvd7ygcvdhnV0JIXt8jyXB95VXWtyS2lBaKTaZiTDeyzMGsb6soawY9AZhXkcU4OuLDNFRGTFBBMdnbExuYfEE5g0DYK2wUcQmOl9aANkkIbHD64jEqAIy5dl6syD+AZ8ZmgiKdcPGUUdF0Pqj4z0+E2sqZ3IxWca4renxJjgqGn7P2T0GizKyrJAtgqIScoaxOAwPYBNxA95EpqSCvd/iuQ7vq5IaTmR4tLrJp7wYi8PwkNokLiAxmTqf9cHuxWmFvjF+tc+xTCwbCxws3ky2XndoDhsycK5h6y85HeD8NiEbbGLaTpfMB09fQ0+f910rOjMbVNp08WcLmUr3fG0SWAe13CYR6A7FbBLIVsduEm5d7XpB64wf4IcYYIWgaAb+KKToIGB71SvbbN7kVW/PCfwg4O3og2es2X4zOQKNeMiPDMMTk2gjceevwI4+DPgZcqlfRoltLnmNilcuztTLAKx3nTaMvgb6W6uLvDw3yfw2E7CHSjMU/Bzkh2QAUEILrNB8MdZzf1EMgTAKeHeSkbnGNfn/G6iSGqco/RMWqP0VYAAkGbZSbTiI4QAAAABJRU5ErkJggg==">';
					  content = content + '</th><th>Удаленный датчик № '+i+'</th><th></th></tr></thead>';
					  content = content + '<tbody class="tempt" id="get_sensor_ip-'+i+'"><tr>';
					  content = content + '<tr class="tempt"><td></td><td>Температура датчика (&deg;C)</td>';
					  content = content + '<td></td>';
					  content = content + '<td id="get_sensorparamip-'+i+':sensor_temp">-_-</td></tr>';
					  content = content + '<tr class="tempt"><td></td><td>Время с последнего обновления (сек)</td>';
					  content = content + '<td></td>';
					  content = content + '<td id="get_sensorparamip-'+i+':stime">-_-</td></tr>';
					  content = content + '<tr class="tempt"><td></td><td>IP адрес датчика</td>';
					  content = content + '<td></td>';
					  content = content + '<td id="get_sensorparamip-'+i+':sensor_ip">-_-</td></tr>';
					  content = content + '<tr class="tempt"><td></td><td>RSSI (дб)</td>';
					  content = content + '<td></td>';
					  content = content + '<td id="get_sensorparamip-'+i+':rssi">-_-</td></tr>';
					  content = content + '<tr class="tempt"><td></td><td>VCC (В)</td>';
					  content = content + '<td></td>';
					  content = content + '<td id="get_sensorparamip-'+i+':vcc">-_-</td></tr>';
					  content = content + '<tr class="tempt"><td></td><td>Количество полученных пакетов</td>';
					  content = content + '<td></td>';
					  content = content + '<td id="get_sensorparamip-'+i+':sensor_count">-_-</td></tr>';
					  content = content + '<tr class="tempt"><td></td><td>Использовать данные датчика</td>';
					  content = content + '<td></td>';
					  content = content + '<td><input style="display:none" type="checkbox" id="get_sensoruseip-'+i+'" onchange="setParam(\'get_sensorUseIP('+i+')\');" ><label for="get_sensoruseip-'+i+'"><span id="get_sensoruseip-'+i+'2"></span></label></td></tr>';
					  content = content + '<tr class="tempt"><td></td><td>Усреднение показаний</td>';
					  content = content + '<td></td>';
					  content = content + '<td><input style="display:none" type="checkbox" id="get_sensorruleip-'+i+'" onchange="setParam(\'get_sensorRuleIP('+i+')\');" ><label for="get_sensorruleip-'+i+'"><span id="get_sensorruleip-'+i+'2"></span></label></td></tr>';
					  content = content + '<tr class="tempt"><td></td><td>Привязать IP датчик к датчику:</td>';
					  content = content + '<td></td>';
					  content = content + '<td><div class="12u$"><div class="select-wrapper"><h7><select id="get_sensorlistip-'+i+'" onChange="setParam(\'get_sensorListIP-'+i+'\',\'get_sensorlistip-'+i+'\');"></select></h7></div></div></td></tr>';
					  content = content + '</tbody></table>';
					  document.getElementById(content_id).innerHTML = content;
                    }
                    updateParam(upsens); //console.log("upsens " + upsens);
                    loadParam(loadsens); //console.log("loadsens: " + loadsens);
                  
                } else if (values[0]=='get_listRelay') { //console.log("get_listRelay!!");
                  content=""; content2=""; upsens = ""; loadsens=""; 
                  var count = values[1].split(';'); console.log("list:" + count);
                    for (var i=0;i<count.length-1;i++) {
                      relay = count[i].toLowerCase();
                      loadsens = loadsens + "get_pinRelay("+count[i]+"),get_presentRelay("+count[i]+"),get_noteRelay("+count[i]+"),";
                      upsens = upsens + "get_Relay("+count[i]+"),";
                      content = content + '<tr class="relay" id="get_presentrelay-'+relay+'"><td>'+count[i]+'</td><td id="get_noterelay-'+relay+'"></td><td id="get_pinrelay-'+relay+'"></td><td><input style="display:none" type="checkbox" id="get_relay-'+relay+'" onchange="setParam(\'get_Relay('+count[i]+')\');" ><label for="get_relay-'+relay+'"><span id="get_relay-'+relay+'2"></span></label></td>';
                      content = content + '</tr>';
					 
                    }
                    document.getElementById(cont2).innerHTML = content; //console.log("get_numberIP-content " + content);
					updateParam(upsens); //console.log("upsens " + upsens);
                    loadParam(loadsens); //console.log("loadsens: " + loadsens);
                  
                } else if (values[0]=='get_listInput') { //console.log("get_listRelay!!");
                  content=""; content2=""; upsens = ""; loadsens=""; 
                  var count = values[1].split(';');// console.log("list:" + count);
                    for (var i=0;i<count.length-1;i++) {
                      input = count[i].toLowerCase();
                      loadsens = loadsens + "get_alarmInput("+count[i]+"),get_errcodeInput("+count[i]+"),get_typeInput("+count[i]+"),get_pinInput("+count[i]+"),get_Input("+count[i]+"),get_noteInput("+count[i]+"),get_testInput("+count[i]+"),";
                      upsens = upsens + "get_Input("+count[i]+"),get_errcodeInput("+count[i]+"),";
                      
					  content = content + '<table><tr id="get_listinput-'+input+'">';
					  content = content + '<tr class="sensor_a"><td>Имя датчика</td><td>'+count[i]+'</td></tr>';
					  content = content + '<tr class="sensor_p"><td>Описание</td><td id="get_noteinput-'+input+'"></td></tr>';
					  content = content + '<tr class="sensor_p"><td>Тип</td><td id="get_typeinput-'+input+'">-</td>';
					  content = content + '<tr class="sensor_p"><td>Значение</td><td id="get_input-'+input+'">-</td></tr>';					  
					  content = content + '<tr class="sensor_p"><td><input class="buttont" type="submit" value="Состояние в режиме аварии" onclick="setParam(\'get_alarmInput('+count[i]+')\');"></td>';
					  content = content + '<td><input id="get_alarminput-'+input+'" type="number" min="0" max="1" step="1" value=""></tr>';					  
					  content = content + '<tr class="sensor_p"><td><input class="buttont" type="submit" value="Тест датчика"  onclick="setParam(\'get_testInput('+count[i]+')\');"></td>';
					  content = content + '<td><input id="get_testinput-'+input+'" type="number" min="0" max="1" step="1" value=""></tr>';					  
					  content = content + '<tr class="sensor_p"><td>Pin</td><td id="get_pininput-'+input+'">-</td>';
					  content = content + '<tr class="sensor_p"><td>Код ошибки</td><td id="get_errcodeinput-'+input+'">-</td></tr>';
					  content = content + '</tr></table><p></p>';
					  
                    }
                    document.getElementById(cont2).innerHTML = content; //console.log("get_numberIP-content " + content);
                    updateParam(upsens); //console.log("upsens " + upsens);
                    loadParam(loadsens); //console.log("loadsens: " + loadsens);
                  
                } else if (values[0]=='get_listTemp') { console.log("get_listTemp!!");
                  content=""; content_t=""; content2=""; upsens = ""; loadsens=""; 
                  var count = values[1].split(';'); console.log("list:" + count);
                    for (var i=0;i<count.length-1;i++) {
                      input = count[i].toLowerCase(); console.log("input:" + count[i]);
                      loadsens = loadsens + "get_nTemp("+count[i]+"),";
                      upsens = upsens + "get_fullTemp("+count[i]+"),"; console.log("get_fullTemp:" + upsens);
					  content_t = "get_fullTemp("+count[i]+")"; //console.log("$$$$$$$$$$$$$$$$$$$$$$$content_t:" + content_t);
                        content = content + '<tr id="get_presenttemp-'+input+'">';
                            content = content + ' <td>'+count[i]+'</td>';
                            content = content + ' <td id="get_ntemp-'+input+'"></td>';
							content = content + ' <td></td>';
							content = content + ' <td id="get_fulltemp-'+input+'">-</td>';
							content = content + '</tr>';
							
                    }
                    document.getElementById(cont2).innerHTML = content; //console.log("get_numberT-content " + content);
                    updateParam(upsens); console.log("upsens " + upsens);
                    loadParam(loadsens); console.log("loadsens: " + loadsens);


                  } else if (values[0]=='get_listFlow') { //console.log("get_listFlow!!");
                  content=""; content2=""; upsens = ""; loadsens=""; 
                  var count = values[1].split(';'); // console.log("list:" + count);                  
                    for (var i=0;i<count.length-1;i++) {
                      input = count[i].toLowerCase();
                      loadsens = loadsens + "get_noteFlow("+count[i]+"),get_Flow("+count[i]+"),get_minFlow("+count[i]+"),get_kfFlow("+count[i]+"),get_frFlow("+count[i]+"),get_testFlow("+count[i]+"),get_pinFlow("+count[i]+"),get_errcodeFlow("+count[i]+"),";
                      upsens = upsens + "get_Flow("+count[i]+"),get_frFlow("+count[i]+"),get_errcodeFlow("+count[i]+"),"; 
					  
                      content = content + '<table><tr id="get_listflow-'+input+'">';
					  content = content + '<tr class="sensor_a"><td>Имя датчика</td><td>'+count[i]+'</td></tr>';
					  content = content + '<tr class="sensor_p"><td>Описание</td><td id="get_noteflow-'+input+'">-</td></tr>';
					  content = content + '<tr class="sensor_p"><td>Значение (м³ в час)</td><td id="get_flow-'+input+'">-</td></tr>';
					  content = content + '<tr class="sensor_p"><td>Минимум (м³ в час)</td><td id="get_minflow-'+input+'">-</td></tr>';
					  content = content + '<tr class="sensor_p"><td>Частота импульсов</td><td id="get_frflow-'+input+'">-</td></tr>';
					  content = content + '<tr class="sensor_p"><td><input class="buttont" type="submit" value="Коэффициент пересчета"  onclick="setParam(\'get_kfFlow('+count[i]+')\');"></td>';
					  content = content + '<td><input id="get_kfflow-'+input+'" type="number" min="0.0" max="1000" step="0.001" value=""></td></tr>';
					  content = content + '<tr class="sensor_p"><td><input class="buttont" type="submit" value="Тест датчика (м³)"  onclick="setParam(\'get_testFlow('+count[i]+')\');"></td>';
                      content = content + '<td><input id="get_testflow-'+input+'" type="number" min="0.0" max="1000" step="0.001" value=""></td></tr>';
					  content = content + '<tr class="sensor_p"><td>Пин</td><td id="get_pinflow-'+input+'">-</td></tr>';
					  content = content + '<tr class="sensor_p"><td>Код ошибки</td><td id="get_errcodeflow-'+input+'">-</td></tr>';
					  content = content + '</tr></table><p></p>';
					  
                    }
                    document.getElementById(cont2).innerHTML = content; //console.log("get_numberIP-content " + content);
                    updateParam(upsens); //console.log("upsens " + upsens);
                    loadParam(loadsens); //console.log("loadsens: " + loadsens);  
          
                     
                } else if (values[0]=='get_Profile(NUM_PROFILE)') { //console.log("get_numberIP!!");
                  content=""; content2=""; upsens = ""; loadsens=""; count = Number(values[1]); //console.log("get_numberIP:"+count);
                    for (var i=0;i<count;i++) {
                      loadsens = loadsens + "infoProfile("+i+"),";
                      content = content + '<tr class="profiles" id="get_profile-'+i+'"><td>'+i+'</td><td id="infoprofile-'+i+'"></td>';
                      content = content + '<td><input class="button2 small" type="submit" value="Стереть"  onclick=\'loadParam("eraseProfile('+i+')")\'></td><td><input class="button2 small" id="load-profile-'+i+'" type="submit" value="Загрузить"  onclick=\'loadParam("loadProfile('+i+')")\'></td></tr>';
                    }
                    document.getElementById(cont2).innerHTML = content; //console.log("get_numberIP-content " + content);
                    //document.getElementById(cont2 + "-inputs").innerHTML = content2; //console.log("get_numberIP-content " + content);
                    loadParam(loadsens); //console.log("loadsens: " + loadsens);
                  
                } else { 
                  cont1 = values[1].replace(/\:$/g,"").replace(/\:/g,"</td><td>"); //console.log("cont1:"+cont1);
                  count=cont2.replace(/[^\d;]/g,""); //console.log("cont2:"+cont2+" count:"+count);
                  var content = '<td>' + cont1 + '</td>'; 
                  var element=document.getElementById(cont2);
                  if(element) {element.innerHTML = content;}
                }
              }
            } else if (type == 'values') { 
                var valueid = values[0].replace(/set_/g,"get_")./*replace(/\([0-9]*\)/g,"").*/replace(/\(/g,"-").replace(/\)/g,"").toLowerCase(); //.toLowerCase().replace(/set_/g,"get_").replace(/\([0-9]\)/g,"");
                var valuevar = values[1].toLowerCase().replace(/[^\w\d]/g,""); //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%valuevar:"+valuevar);
                if (valueid != null  && valueid != ""  && values[1] != null) {
                if ( valueid == "get_message-sms_ret") {  
                    if(valuevar == "waitresponse") {  setTimeout(loadParam('get_Message(SMS_RET)'), 3000); console.log("wait response..."); } else { alert(values[1]); }
                  }               
                if ( valueid == "get_message-mail_ret") {  
                    if(valuevar == "waitresponse") {  setTimeout(loadParam('get_Message(MAIL_RET)'), 3000); console.log("wait response..."); } else { alert(values[1]); }
                  }
				  
				if (valueid == "get_fulltemp-tout")
					{					
					if (values[1] <= 0) {
					document.getElementById("get_fulltemp-tout").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_fulltemp-tout").style.color = "#e44c65";
					}
					}
					
				if (valueid == "get_rawtemp-tout")
					{					
					if (values[1] <= 0) {
					document.getElementById("get_rawtemp-tout").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_rawtemp-tout").style.color = "#e44c65";
					}
					}
				
				if (valueid == "get_fulltemp-tin")
					{					
					if (values[1] <= 0) {
					document.getElementById("get_fulltemp-tin").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_fulltemp-tin").style.color = "#e44c65";
					}
					}
					
					if (valueid == "get_rawtemp-tin")
					{					
					if (values[1] <= 0) {
					document.getElementById("get_rawtemp-tin").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_rawtemp-tin").style.color = "#e44c65";
					}
					}
				
				if (valueid == "get_fulltemp-tevain") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%valuevar:"+valuevar);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_fulltemp-tevain").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_fulltemp-tevain").style.color = "#e44c65";
					}
					}
					
					if (valueid == "get_rawtemp-tevain") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%valuevar:"+valuevar);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_rawtemp-tevain").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_rawtemp-tevain").style.color = "#e44c65";
					}
					}
				
				if (valueid == "get_fulltemp-tevaout") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%valuevar:"+valuevar);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_fulltemp-tevaout").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_fulltemp-tevaout").style.color = "#e44c65";
					}
					}
					
					if (valueid == "get_rawtemp-tevaout") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%valuevar:"+valuevar);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_rawtemp-tevaout").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_rawtemp-tevaout").style.color = "#e44c65";
					}
					}
				
				if (valueid == "get_fulltemp-tconin") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_fulltemp-tconin").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_fulltemp-tconin").style.color = "#e44c65";
					}
					}
					
					if (valueid == "get_rawtemp-tconin") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_rawtemp-tconin").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_rawtemp-tconin").style.color = "#e44c65";
					}
					}
				
				if (valueid == "get_fulltemp-tconout") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_fulltemp-tconout").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_fulltemp-tconout").style.color = "#e44c65";
					}
					}
					
					if (valueid == "get_rawtemp-tconout") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_rawtemp-tconout").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_rawtemp-tconout").style.color = "#e44c65";
					}
					}
				
				if (valueid == "get_fulltemp-tconout") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_fulltemp-tconout").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_fulltemp-tconout").style.color = "#e44c65";
					}
					}
					
					if (valueid == "get_rawtemp-tconout") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_rawtemp-tconout").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_rawtemp-tconout").style.color = "#e44c65";
					}
					}
				
				if (valueid == "get_fulltemp-tboiler") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_fulltemp-tboiler").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_fulltemp-tboiler").style.color = "#e44c65";
					}
					}
					
					if (valueid == "get_rawtemp-tboiler") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_rawtemp-tboiler").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_rawtemp-tboiler").style.color = "#e44c65";
					}
					}
				
				if (valueid == "get_fulltemp-taccum") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_fulltemp-taccum").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_fulltemp-taccum").style.color = "#e44c65";
					}
					}
					
					if (valueid == "get_rawtemp-taccum") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_rawtemp-taccum").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_rawtemp-taccum").style.color = "#e44c65";
					}
					}
				
				if (valueid == "get_fulltemp-trtoout") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_fulltemp-trtoout").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_fulltemp-trtoout").style.color = "#e44c65";
					}
					}
					
					if (valueid == "get_rawtemp-trtoout") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_rawtemp-trtoout").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_rawtemp-trtoout").style.color = "#e44c65";
					}
					}
				
				if (valueid == "get_fulltemp-tcomp") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_fulltemp-tcomp").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_fulltemp-tcomp").style.color = "#e44c65";
					}
					}
					
					if (valueid == "get_rawtemp-tcomp") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_rawtemp-tcomp").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_rawtemp-tcomp").style.color = "#e44c65";
					}
					}
				
				if (valueid == "get_fulltemp-tevaing") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_fulltemp-tevaing").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_fulltemp-tevaing").style.color = "#e44c65";
					}
					}
					
					if (valueid == "get_rawtemp-tevaing") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_rawtemp-tevaing").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_rawtemp-tevaing").style.color = "#e44c65";
					}
					}
				
				if (valueid == "get_fulltemp-tevaoutg") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_fulltemp-tevaoutg").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_fulltemp-tevaoutg").style.color = "#e44c65";
					}
					}
					
					if (valueid == "get_rawtemp-tevaoutg") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_rawtemp-tevaoutg").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_rawtemp-tevaoutg").style.color = "#e44c65";
					}
					}
				
				if (valueid == "get_fulltemp-tconing") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_fulltemp-tconing").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_fulltemp-tconing").style.color = "#e44c65";
					}
					}
					
					if (valueid == "get_rawtemp-tconing") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_rawtemp-tconing").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_rawtemp-tconing").style.color = "#e44c65";
					}
					}
				
				if (valueid == "get_fulltemp-tconoutg") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_fulltemp-tconoutg").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_fulltemp-tconoutg").style.color = "#e44c65";
					}
					}
					
					if (valueid == "get_rawtemp-tconoutg") //console.log("!!!!!!!!!!!!valueid:"+valueid+" %%%%%%%%%%%%values[1]:"+values[1]);
					{					
					if (values[1] <= 0) {
					document.getElementById("get_rawtemp-tconoutg").style.color = "#00b1ff";
					}
					else {
					document.getElementById("get_rawtemp-tconoutg").style.color = "#e44c65";
					}
					}
								

                if (values[1] == "Alarm"  ) { document.getElementById(valueid.replace(/type/g,"alarm")).disabled = false; }
                else if (values[1] == "Work") { document.getElementById(valueid.replace(/type/g,"alarm")).disabled = true; }
				
				                          
                  element3=document.getElementById(valueid+"3");  if(element3)  {element3.value  = values[1];  element3.innerHTML = values[1];}
                  element=document.getElementById(valueid);
                  //console.log("element: " + element + "<-->"  + valueid );
                  if(element) {if(element!=document.activeElement) {element.value = values[1]; element.innerHTML = values[1];}}
                  if(reerr.test(values[1]))   { element=document.getElementById(valueid); if(element) {element.placeholder = values[1];} } }            
                  if(remintemp.test(valueid)) { document.getElementById(valueid.replace(/get_min/g,"get_test")).min = values[1]; }
                  else if (remaxtemp.test(valueid)) { document.getElementById(valueid.replace(/get_max/g,"get_test")).max = values[1]; }  
                
                if ( valueid == "get_paramheathp-k_weather" || valueid == "get_paramheathp-temp_pid" ) { 
                  calctpod("heat"); console.log("valuevar:"+valuevar);
                } else if ( valueid == "get_paramcoolhp-k_weather" || valueid == "get_paramcoolhp-temp_pid" ) {
                  calctpod("cool"); console.log("valuevar:"+valuevar);
                }
				
                   
            } else if (type == 'present') {
                var valueid = values[0].replace(/\(/g,"-").replace(/\)/g,"").toLowerCase();
                if (valueid != null  && values[1] != null && values[1] == 0 ) {
                  element=document.getElementById(valueid);if(element) {element.className = "inactive";}
                  if(values[0] == "get_presentRelay(RHEAT)" && values[1] == 0) {element=document.getElementById("get_optionhp-add_heat");if(element) {element.disabled = true;}}
                  if(values[0] == "get_presentRelay(REVI)" && values[1] == 0) {element=document.getElementById("get_optionhp-temp_evi");if(element) {element.disabled = true;} element=document.getElementById("get_optionhp-temp_evi2");if(element) {element.disabled = true;}}
                  var tableElem = document.getElementById(valueid);
                  if (tableElem) {
                    var elements = tableElem.getElementsByTagName('select');
                    if(elements[0]) {elements[0].disabled = true;}
                  }
                }
            } else if (type == 'checkbox') { //console.log("<-!!!!!!!!!!!!!!!!!!!!!->" + type + ": " + values[0] + "<-!!!!!!!!!!!!!!!!!!!!!->"  + values[1] );

                var valueid = values[0].replace(/\(/g,"-").replace(/\)/g,"").replace(/set_/g,"get_").toLowerCase();
                    if(values[1] == 1) { console.log("valueid###############: " + valueid);
                      var element=document.getElementById(valueid);
                      if(element) {document.getElementById(valueid).checked = true;}
                      var element2=document.getElementById(valueid+"2");
                      if(element2) {element2.innerHTML = '<img src="on.png" width="30%" alt="" />';}
					  var element4=document.getElementById(valueid+"3");
				  	  if(element4) {document.getElementById(valueid+"3").innerHTML = '<img src="shem.png" alt="">'; document.getElementById("get_temp-tconoutg_t").style.left ="43%";}
					
					} else if (values[1] == 0) { //console.log("valueid$$$$$$$$$$$$$$: " + valueid);
                      var element=document.getElementById(valueid);
                      if(element) {document.getElementById(valueid).checked = false;}
                      var element2=document.getElementById(valueid+"2");
                      if(element2) {element2.innerHTML = '<img src="off.png" width="30%" alt="" />';}
					  var element4=document.getElementById(valueid+"3");
				  	  if(element4) {document.getElementById(valueid+"3").innerHTML = '<img src="shem1.png" alt="">'; document.getElementById("get_temp-tconoutg_t").style.left ="17%";}
					  }
					
			} else {                
                if (values[0] == "get_WORK" &&  values[1] == "ON" ) {  //console.log(type + ": " + values[0] + "<-->"  + values[1] );
				  var element2=document.getElementById("get_work2");
				  if(element2) {document.getElementById("get_work2").innerHTML = '<input class="button3 small" type="submit" value="" onClick="swich(\'onoffswitch\')" /><input type="submit" value="ТН включен. Выключить ТН?" onClick="swich(\'onoffswitch\')" class="button small" />';}
				  var element=document.getElementById("onoffswitch");
                  if(!element) {document.getElementById(values[0].toLowerCase()).innerHTML = "ON";}
                  else {document.getElementById("onoffswitch").checked = false;
				  
				  document.getElementById("get_work2").innerHTML = '<input class="button3 small" type="submit" value="" onClick="swich(\'onoffswitch\')" /><input type="submit" value="ТН включен. Выключить ТН?" onClick="swich(\'onoffswitch\')" class="button small" />';
				  }
				  
					//if(document.getElementById('chart1')) document.getElementById("chart1").innerHTML ="<div id=\"chart1\">График не выбран</div><table><tbody><tr><td></td><td>График: </td><td><div class=\"12u$\"><div class=\"select-wrapper\"><h7><select id=\"get_listchart\")\"><option value=\"_TOUT\"></option>_TOUT</select></h7></div></div></td></tr></tbody></table>";
					if(document.getElementById('m-test2')) document.getElementById("m-test2").innerHTML = "";
					if(document.getElementById('m-control2')) document.getElementById("m-control2").style.display = "block";
					if(document.getElementById('m-test')) document.getElementById("m-test").innerHTML = "Режим \"Тест\" неактивен. <br> Для активации ВЫКЛЮЧИТЕ ТН.";
					if(document.getElementById('m-control')) document.getElementById("m-control").style.display = "none";			  
                    if(document.getElementById('get_relay-rcomp')) document.getElementById('get_relay-rcomp').disabled = true;
                    if(document.getElementById('get_relay-rpumpi')) document.getElementById('get_relay-rpumpi').disabled = true;
                    if(document.getElementById('get_relay-rpumpo')) document.getElementById('get_relay-rpumpo').disabled = true;
                    if(document.getElementById('get_relay-rboiler')) document.getElementById('get_relay-rboiler').disabled = true;
                    if(document.getElementById('get_relay-rheat')) document.getElementById('get_relay-rheat').disabled = true;
                    if(document.getElementById('get_relay-rtrv')) document.getElementById('get_relay-rtrv').disabled = true;
                    if(document.getElementById('get_relay-rfan1')) document.getElementById('get_relay-rfan1').disabled = true;
                    if(document.getElementById('get_relay-rfan2')) document.getElementById('get_relay-rfan2').disabled = true;
                    if(document.getElementById('get_relay-r3way')) document.getElementById('get_relay-r3way').disabled = true;
                    if(document.getElementById('get_relay-revi')) document.getElementById('get_relay-revi').disabled = true;
                    if(document.getElementById('get_relay-rpumpb')) document.getElementById('get_relay-rpumpb').disabled = true;
                    if(document.getElementById('get_eev')) document.getElementById('get_eev').disabled = true;
                    if(document.getElementById('get_eev3')) document.getElementById('get_eev3').disabled = true;
                    if(document.getElementById('get_zeroEEV')) document.getElementById('get_zeroEEV').disabled = true;
                    if(document.getElementById('get_paramfc-on_off')) document.getElementById('get_paramfc-on_off').disabled = true;
                    //if(document.getElementById('get_fc')) document.getElementById('get_fc').disabled = true;
                    //if(document.getElementById('get_fc2')) document.getElementById('get_fc2').disabled = true;    
                    if(document.getElementById('get_testmode')) document.getElementById('get_testmode').disabled = true;
                    if(document.getElementById('get_listprofile')) document.getElementById('get_listprofile').disabled = true;
                    if(document.getElementById('load-profile-0')) document.getElementById('load-profile-0').disabled = true;
                    if(document.getElementById('load-profile-1')) document.getElementById('load-profile-1').disabled = true;
                    if(document.getElementById('load-profile-2')) document.getElementById('load-profile-2').disabled = true;
                    if(document.getElementById('load-profile-3')) document.getElementById('load-profile-3').disabled = true;
                    if(document.getElementById('load-profile-4')) document.getElementById('load-profile-4').disabled = true;
                    if(document.getElementById('load-profile-5')) document.getElementById('load-profile-5').disabled = true;
                    if(document.getElementById('load-profile-6')) document.getElementById('load-profile-6').disabled = true;
                    if(document.getElementById('load-profile-7')) document.getElementById('load-profile-7').disabled = true;                    
                    if(document.getElementById('get_modehp')) document.getElementById('get_modehp').disabled = true;
                    if(document.getElementById('scan')) document.getElementById('scan').disabled = true; 
					
					
					
                } else if ( values[0] == "get_WORK" &&  values[1] == "OFF" ) { //console.log(type + ": " + values[0] + "<-->"  + values[1] );
                  var element2=document.getElementById("get_work2");
				  if(element2) {document.getElementById("get_work2").innerHTML = '<input class="button4 small" type="submit" value="" onClick="swich(\'onoffswitch\')" /><input type="submit" value="ТН выключен. Включить ТН?" onClick="swich(\'onoffswitch\')" class="button small" />';}
				  var element=document.getElementById("onoffswitch");
                  if(!element) {document.getElementById(values[0].toLowerCase()).innerHTML = "OFF";}
                  else {document.getElementById("onoffswitch").checked = true;
				  
				  document.getElementById("get_work2").innerHTML = '<input class="button4 small" type="submit" value="" onClick="swich(\'onoffswitch\')" /><input type="submit" value="ТН выключен. Включить ТН?" onClick="swich(\'onoffswitch\')" class="button small" />';
				  }

                    if(document.getElementById('m-test2')) document.getElementById("m-test2").innerHTML = "Графики не доступны.<br> Для отображения включите ТН";
					if(document.getElementById('m-control2')) document.getElementById("m-control2").style.display = "none";
					if(document.getElementById('m-test')) document.getElementById("m-test").innerHTML = "Режим \"Тест\" активирован. <br> НАСТРОЙКА РЕЖИМА \"ТЕСТ\"";
					if(document.getElementById('m-control')) document.getElementById("m-control").style.display = "block";
					if(document.getElementById('get_relay-rcomp')) document.getElementById('get_relay-rcomp').disabled = false;
                    if(document.getElementById('get_relay-rpumpi')) document.getElementById('get_relay-rpumpi').disabled = false;
                    if(document.getElementById('get_relay-rpumpo')) document.getElementById('get_relay-rpumpo').disabled = false;
                    //if(document.getElementById('get_relay-rboiler')) document.getElementById('get_relay-rboiler').disabled = false;
                    if(document.getElementById('get_relay-rheat')) document.getElementById('get_relay-rheat').disabled = false;
                    if(document.getElementById('get_relay-rtrv')) document.getElementById('get_relay-rtrv').disabled = false;
                    if(document.getElementById('get_relay-rfan1')) document.getElementById('get_relay-rfan1').disabled = false;
                    if(document.getElementById('get_relay-rfan2')) document.getElementById('get_relay-rfan2').disabled = false;
                    if(document.getElementById('get_relay-r3way')) document.getElementById('get_relay-r3way').disabled = false;
                    if(document.getElementById('get_relay-revi')) document.getElementById('get_relay-revi').disabled = false;
                    if(document.getElementById('get_relay-rpumpb')) document.getElementById('get_relay-rpumpb').disabled = false;
                    if(document.getElementById('get_eev')) document.getElementById('get_eev').disabled = false;
                    if(document.getElementById('get_eev3')) document.getElementById('get_eev3').disabled = false;
                    if(document.getElementById('get_zeroEEV')) document.getElementById('get_zeroEEV').disabled = false;
                    if(document.getElementById('get_paramfc-on_off')) document.getElementById('get_paramfc-on_off').disabled = false;
                    if(document.getElementById('get_fc')) document.getElementById('get_fc').disabled = false;
                    if(document.getElementById('get_fc2')) document.getElementById('get_fc2').disabled = false; 
                    if(document.getElementById('get_testmode')) document.getElementById('get_testmode').disabled = false;
                    if(document.getElementById('get_listprofile')) document.getElementById('get_listprofile').disabled = false;
                    if(document.getElementById('load-profile-0')) document.getElementById('load-profile-0').disabled = false;
                    if(document.getElementById('load-profile-1')) document.getElementById('load-profile-1').disabled = false;
                    if(document.getElementById('load-profile-2')) document.getElementById('load-profile-2').disabled = false;
                    if(document.getElementById('load-profile-3')) document.getElementById('load-profile-3').disabled = false;
                    if(document.getElementById('load-profile-4')) document.getElementById('load-profile-4').disabled = false;
                    if(document.getElementById('load-profile-5')) document.getElementById('load-profile-5').disabled = false;
                    if(document.getElementById('load-profile-6')) document.getElementById('load-profile-6').disabled = false;
                    if(document.getElementById('load-profile-7')) document.getElementById('load-profile-7').disabled = false;
                    if(document.getElementById('get_modehp')) document.getElementById('get_modehp').disabled = false;
                    if(document.getElementById('scan')) document.getElementById('scan').disabled = false; 
                
				
								
				} else if ( values[0] == "get_MODE" &&  values[1] == "Выключен" ) { //console.log(type + ": " + values[0] + "<-->"  + values[1] );
                  var element1=document.getElementById("get_work5");
				  var element2=document.getElementById("get_mode");
				  if(element1) {document.getElementById('get_mode').innerHTML = values[1] ;}
				  if(element1) {document.getElementById('get_work5').innerHTML = '<img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB4AAAAeCAYAAAA7MK6iAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAAnVJREFUeNrElz1M21AQx88mbSEMzcTasCPBBKvDjpSRGWZQszNkYyyCmR22SDCTrDC5EjvpykRVEkr46v1P76yzndQO5OOkJ8v2vfe7dx/PZ49yyF21+o0vVTdWeJSSKjxCHg2MUqPxK2tNLwO4zJcDHgENJy0e39mAn0OBGfiVL3VMpo8JjK6zAb8zwQ7aci4dhSAEQRLujRk6EO5NANoX7psX9TFCya1dj+3YZW9otfxymQpLS+TNz7+L8tbp0PP1Nb222ykDkO0Fk30RcG57W6CjEMAfjo+tAWBVPHc4yNNPlYpAsUtMeDw7o+erq2iRwuoqfdnYGNoo7B7wp2ZTH5UL7jSKdgoolHrn52nr2QgMGFjc3U29f729JX9hIQXFmlj75eZGd171FaxQ7LIf1Aos/3tyEnsGY//UarK4hd7v7VH38DCCOxHwiiYSFJMLDhIYCH2VmcVFom5XQIAr1CYXGGCBCVeXNGaIKybnEtZ7urykz+vrcqvXh6MjAcLlgCbDAlav3S5JHWvJWDflEcTUCuCzW1tiFKBIxmQuKMsf5QkB9/YuLmKlNGgzvk6Qm0RGZonVtzGVstzZicXc6in4TmKLOl5bIyoW81FZT/Q15Jy5NqZwu8I7+/sxL4CJ5Ap5QoAHCDwOiMfT00wu9OxxOru5ST32gCmZKOEk4x3UZXnou3ZF6hBuwAKw+n+C99CzgsUt1MLxTk8vJ42BRyZKBTVt6xA1KIYZF7/3yNSvU1P7qgl8JFr8dapM7bNoO5AfI2juMps/htb6dSDhGKFhqgOZarM36fY2dVY7hcC2Qx9s6INcDf1Uf2Em8dP2T4ABAOewdj0mdVUHAAAAAElFTkSuQmCC" alt="" />';}
				  
				  		
				
				} else if ( values[0] == "get_MODE" &&  values[1] == "Выключен" ) { //console.log(type + ": " + values[0] + "<-->"  + values[1] );
                  var element1=document.getElementById("get_work5");
				  var element2=document.getElementById("get_mode");
				  if(element1) {document.getElementById('get_mode').innerHTML = values[1] ;}
				  if(element1) {document.getElementById('get_work5').innerHTML = '<img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB4AAAAeCAYAAAA7MK6iAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAAnVJREFUeNrElz1M21AQx88mbSEMzcTasCPBBKvDjpSRGWZQszNkYyyCmR22SDCTrDC5EjvpykRVEkr46v1P76yzndQO5OOkJ8v2vfe7dx/PZ49yyF21+o0vVTdWeJSSKjxCHg2MUqPxK2tNLwO4zJcDHgENJy0e39mAn0OBGfiVL3VMpo8JjK6zAb8zwQ7aci4dhSAEQRLujRk6EO5NANoX7psX9TFCya1dj+3YZW9otfxymQpLS+TNz7+L8tbp0PP1Nb222ykDkO0Fk30RcG57W6CjEMAfjo+tAWBVPHc4yNNPlYpAsUtMeDw7o+erq2iRwuoqfdnYGNoo7B7wp2ZTH5UL7jSKdgoolHrn52nr2QgMGFjc3U29f729JX9hIQXFmlj75eZGd171FaxQ7LIf1Aos/3tyEnsGY//UarK4hd7v7VH38DCCOxHwiiYSFJMLDhIYCH2VmcVFom5XQIAr1CYXGGCBCVeXNGaIKybnEtZ7urykz+vrcqvXh6MjAcLlgCbDAlav3S5JHWvJWDflEcTUCuCzW1tiFKBIxmQuKMsf5QkB9/YuLmKlNGgzvk6Qm0RGZonVtzGVstzZicXc6in4TmKLOl5bIyoW81FZT/Q15Jy5NqZwu8I7+/sxL4CJ5Ap5QoAHCDwOiMfT00wu9OxxOru5ST32gCmZKOEk4x3UZXnou3ZF6hBuwAKw+n+C99CzgsUt1MLxTk8vJ42BRyZKBTVt6xA1KIYZF7/3yNSvU1P7qgl8JFr8dapM7bNoO5AfI2juMps/htb6dSDhGKFhqgOZarM36fY2dVY7hcC2Qx9s6INcDf1Uf2Em8dP2T4ABAOewdj0mdVUHAAAAAElFTkSuQmCC" alt="" />';}
				  } else if ( values[0] == "get_MODE" &&  values[1] == "Останов..." ) { //console.log(type + ": " + values[0] + "<-->"  + values[1] );
                  var element1=document.getElementById("get_work5");
				  var element2=document.getElementById("get_mode");
				  if(element1) {document.getElementById('get_mode').innerHTML = values[1] ;}
				  if(element1) {document.getElementById('get_work5').innerHTML = '<img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB4AAAAeCAYAAAA7MK6iAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAAnVJREFUeNrElz1M21AQx88mbSEMzcTasCPBBKvDjpSRGWZQszNkYyyCmR22SDCTrDC5EjvpykRVEkr46v1P76yzndQO5OOkJ8v2vfe7dx/PZ49yyF21+o0vVTdWeJSSKjxCHg2MUqPxK2tNLwO4zJcDHgENJy0e39mAn0OBGfiVL3VMpo8JjK6zAb8zwQ7aci4dhSAEQRLujRk6EO5NANoX7psX9TFCya1dj+3YZW9otfxymQpLS+TNz7+L8tbp0PP1Nb222ykDkO0Fk30RcG57W6CjEMAfjo+tAWBVPHc4yNNPlYpAsUtMeDw7o+erq2iRwuoqfdnYGNoo7B7wp2ZTH5UL7jSKdgoolHrn52nr2QgMGFjc3U29f729JX9hIQXFmlj75eZGd171FaxQ7LIf1Aos/3tyEnsGY//UarK4hd7v7VH38DCCOxHwiiYSFJMLDhIYCH2VmcVFom5XQIAr1CYXGGCBCVeXNGaIKybnEtZ7urykz+vrcqvXh6MjAcLlgCbDAlav3S5JHWvJWDflEcTUCuCzW1tiFKBIxmQuKMsf5QkB9/YuLmKlNGgzvk6Qm0RGZonVtzGVstzZicXc6in4TmKLOl5bIyoW81FZT/Q15Jy5NqZwu8I7+/sxL4CJ5Ap5QoAHCDwOiMfT00wu9OxxOru5ST32gCmZKOEk4x3UZXnou3ZF6hBuwAKw+n+C99CzgsUt1MLxTk8vJ42BRyZKBTVt6xA1KIYZF7/3yNSvU1P7qgl8JFr8dapM7bNoO5AfI2juMps/htb6dSDhGKFhqgOZarM36fY2dVY7hcC2Qx9s6INcDf1Uf2Em8dP2T4ABAOewdj0mdVUHAAAAAElFTkSuQmCC" alt="" />';}
				  
				  } else if ( values[0] == "get_MODE" &&  values[1] == "Пуск..." ) { //console.log(type + ": " + values[0] + "<-->"  + values[1] );
                  var element1=document.getElementById("get_work5");
				  var element2=document.getElementById("get_mode");
				  if(element1) {document.getElementById('get_mode').innerHTML = values[1] ;}
				  if(element1) {document.getElementById('get_work5').innerHTML = '<img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB4AAAAeCAYAAAA7MK6iAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAAotJREFUeNrElz1IHEEUx99uDk7uIBcFzwRMcpJoI6KFZxEs9jDdpRACFnbWsUhKq1yqQJqkSOqkshAUmyvlrggposUGuSIxIRcJJJ6gbkDxIAHnP8xs5sbbL+7rwbDL7uz7zfuYN28NCiHPPzy8zS7zYkyxcU2bcsKGzcYmxsq99R9BOo0A4CS7vGLDomhSZuMxW8CnSGAGTLFLAR9Ta4JFF9gCnECwgJaFS9shCIGlw40OQz3hRhegTeGm8qLQQSgJ3YUGi0X22s1mpxMZ6oslIxHO/55S7azquQBke0zJPldS8UGaHV6giXSuJRN3ayV6/3ONnPqhnuk5QxQHd3kTgxbNZZYiW3lw+p32jrb5fTyWoLH+GUr1pbn1W9W3tHtYVqdnYqIacRntz1L+7nJkt258fkH7fyoNz7eq72j6ep5mby5wnbBamTNvquD7zNIwln05+uhCVytPL0Gl7PwuUvHra36fv/NIfcXBU9LFcE0QFCBYiPudX0W/JOKyd7zNYw3dYMgEM2XBv3V13FUOS5q5FND6vzOuYCg5wuJWChUO6SHJANP8n8lpcs5rXDmGCtehMg+0bPUUGQowpKgFhGVjkr+E+2RsJBTPVGgUwYJ1ccFOvca30OL4M140EBvA/aDxK4lQYOliMFTwieoOFY6952fp2MBMKPDoQLbB5WCaslQCghjrcD/3oroFWQ0d2RsPuG6liNimaFe4FL+9cT+QcL+YYotgDkqsFxTvdd1gtqVkIgGxtfadCk8keAFhQK33KpnydCqpfVWHD4kyO51yPTsW1Q7kZRuau8Dmj0GfNOtA7A5C7UsdSE+bvW63t6Y+S0yw9HaohYbeCtXQ9/QXphs/bRcCDACRVV7sbpUreAAAAABJRU5ErkJggg==" alt="" />';}
				  
				  } else if ( values[0] == "get_MODE" &&  values[1] == "Пауза" ) { //console.log(type + ": " + values[0] + "<-->"  + values[1] );
                  var element1=document.getElementById("get_work5");
				  var element2=document.getElementById("get_mode");
				  if(element1) {document.getElementById('get_mode').innerHTML = values[1] ;}
				  if(element1) {document.getElementById('get_work5').innerHTML = '<img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB4AAAAeCAYAAAA7MK6iAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAApBJREFUeNrMV79v01AQvhhLyRZPpBPOBCw0XlBZkjgCWsYMQAag/AvZkaAUwZx/gQISYyegBYEjFtIuSVlappiJMLlbsvG+x6V17edfkRL4pJOf7p3f5/fu7t05RynwdPO5KR5NFkuIETDxhPSFbEOePH7kJq2ZSyCsiEdHiE3Z4Ahpiw8YZCIWhEXx2MDLqnnTvECVyjIV8nnq7e2T6/6MWh8fvSE+4Dg4cS6C1OFjDaGyfIVarTvkeR4VCgWy6zU5Ho1+q8yvCbnVaFx/63z5PIkk9pFaUVto3b1Nvd4+vXv/gQYH36VuZeWq3HkEllTkWhZSwDAMGrqnsYMxdAnAmg5znCVmn1o0P1jMcUrM0dum+aPNXKT7ok+iXqtSvV5VvrX57EXsqusP7lG5bIb0w6FLW6/e+CO9ofPlYPt9Rt3ZtjMYHChTC1Hvgw1OPZg2eDEmL+OJOcpToKlF5euc0dSDkZzGx0axSOVaNXSMKX0sI1wPXvhYCIZJuTz9uG7364n+12iktFfoDV3lpwy+OoPd3U+pbTX6R9BVRQCVR4WAn0JYXb1BS6WS8qiDp6FxEV80PJ07B3vBPu5r3K6kQql0fqY5BbZTE+dFt/Fw/T71xbU4Ho9P9BhDhznYpCbmxsxJrGkccEdHP2ht7SZ96+1JwRg6v01SPwbOaVS32deRQJ91KAgQ9SD0+xM6zMEmphMhH9ffPOZusLOAaO5MO89gBxK5a1Ssy5cuymPFkcp8F4IxdJhLqGr9UAfCuz7mtFKSI4DkkYsisLPzUV4yEIxNLgxTmwhS29/m5mLaW0uVMohe4JADCjsFXm69jmpxQ6QzNfRIGRwvAmnqAux0Mplkauj/r1+YRfy0/RFgAL1gLsiwueo4AAAAAElFTkSuQmCC" alt="" />';}
				  
				  } else if ( values[0] == "get_MODE" &&  values[1] == "Нагрев ГВС." ) { //console.log(type + ": " + values[0] + "<-->"  + values[1] );
                  var element1=document.getElementById("get_work5");
				  var element2=document.getElementById("get_mode");
				  if(element1) {document.getElementById('get_mode').innerHTML = values[1] ;}
				  if(element1) {document.getElementById('get_work5').innerHTML = '<img src="data:image/png;base64,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" alt="" />';}
				  } else if ( values[0] == "get_MODE" &&  values[1] == "ГВС" ) { //console.log(type + ": " + values[0] + "<-->"  + values[1] );
                  var element1=document.getElementById("get_work5");
				  var element2=document.getElementById("get_mode");
				  if(element1) {document.getElementById('get_mode').innerHTML = values[1] ;}
				  if(element1) {document.getElementById('get_work5').innerHTML = '<img src="data:image/png;base64,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" alt="" />';}
				  
				  } else if ( values[0] == "get_MODE" &&  values[1] == "Нагрев" ) { //console.log(type + ": " + values[0] + "<-->"  + values[1] );
                  var element1=document.getElementById("get_work5");
				  var element2=document.getElementById("get_mode");
				  if(element1) {document.getElementById('get_mode').innerHTML = values[1] ;}
				  if(element1) {document.getElementById('get_work5').innerHTML = '<img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAB4AAAAeCAYAAAA7MK6iAAAACXBIWXMAAAsTAAALEwEAmpwYAAAKT2lDQ1BQaG90b3Nob3AgSUNDIHByb2ZpbGUAAHjanVNnVFPpFj333vRCS4iAlEtvUhUIIFJCi4AUkSYqIQkQSoghodkVUcERRUUEG8igiAOOjoCMFVEsDIoK2AfkIaKOg6OIisr74Xuja9a89+bN/rXXPues852zzwfACAyWSDNRNYAMqUIeEeCDx8TG4eQuQIEKJHAAEAizZCFz/SMBAPh+PDwrIsAHvgABeNMLCADATZvAMByH/w/qQplcAYCEAcB0kThLCIAUAEB6jkKmAEBGAYCdmCZTAKAEAGDLY2LjAFAtAGAnf+bTAICd+Jl7AQBblCEVAaCRACATZYhEAGg7AKzPVopFAFgwABRmS8Q5ANgtADBJV2ZIALC3AMDOEAuyAAgMADBRiIUpAAR7AGDIIyN4AISZABRG8lc88SuuEOcqAAB4mbI8uSQ5RYFbCC1xB1dXLh4ozkkXKxQ2YQJhmkAuwnmZGTKBNA/g88wAAKCRFRHgg/P9eM4Ors7ONo62Dl8t6r8G/yJiYuP+5c+rcEAAAOF0ftH+LC+zGoA7BoBt/qIl7gRoXgugdfeLZrIPQLUAoOnaV/Nw+H48PEWhkLnZ2eXk5NhKxEJbYcpXff5nwl/AV/1s+X48/Pf14L7iJIEyXYFHBPjgwsz0TKUcz5IJhGLc5o9H/LcL//wd0yLESWK5WCoU41EScY5EmozzMqUiiUKSKcUl0v9k4t8s+wM+3zUAsGo+AXuRLahdYwP2SycQWHTA4vcAAPK7b8HUKAgDgGiD4c93/+8//UegJQCAZkmScQAAXkQkLlTKsz/HCAAARKCBKrBBG/TBGCzABhzBBdzBC/xgNoRCJMTCQhBCCmSAHHJgKayCQiiGzbAdKmAv1EAdNMBRaIaTcA4uwlW4Dj1wD/phCJ7BKLyBCQRByAgTYSHaiAFiilgjjggXmYX4IcFIBBKLJCDJiBRRIkuRNUgxUopUIFVIHfI9cgI5h1xGupE7yAAygvyGvEcxlIGyUT3UDLVDuag3GoRGogvQZHQxmo8WoJvQcrQaPYw2oefQq2gP2o8+Q8cwwOgYBzPEbDAuxsNCsTgsCZNjy7EirAyrxhqwVqwDu4n1Y8+xdwQSgUXACTYEd0IgYR5BSFhMWE7YSKggHCQ0EdoJNwkDhFHCJyKTqEu0JroR+cQYYjIxh1hILCPWEo8TLxB7iEPENyQSiUMyJ7mQAkmxpFTSEtJG0m5SI+ksqZs0SBojk8naZGuyBzmULCAryIXkneTD5DPkG+Qh8lsKnWJAcaT4U+IoUspqShnlEOU05QZlmDJBVaOaUt2ooVQRNY9aQq2htlKvUYeoEzR1mjnNgxZJS6WtopXTGmgXaPdpr+h0uhHdlR5Ol9BX0svpR+iX6AP0dwwNhhWDx4hnKBmbGAcYZxl3GK+YTKYZ04sZx1QwNzHrmOeZD5lvVVgqtip8FZHKCpVKlSaVGyovVKmqpqreqgtV81XLVI+pXlN9rkZVM1PjqQnUlqtVqp1Q61MbU2epO6iHqmeob1Q/pH5Z/YkGWcNMw09DpFGgsV/jvMYgC2MZs3gsIWsNq4Z1gTXEJrHN2Xx2KruY/R27iz2qqaE5QzNKM1ezUvOUZj8H45hx+Jx0TgnnKKeX836K3hTvKeIpG6Y0TLkxZVxrqpaXllirSKtRq0frvTau7aedpr1Fu1n7gQ5Bx0onXCdHZ4/OBZ3nU9lT3acKpxZNPTr1ri6qa6UbobtEd79up+6Ynr5egJ5Mb6feeb3n+hx9L/1U/W36p/VHDFgGswwkBtsMzhg8xTVxbzwdL8fb8VFDXcNAQ6VhlWGX4YSRudE8o9VGjUYPjGnGXOMk423GbcajJgYmISZLTepN7ppSTbmmKaY7TDtMx83MzaLN1pk1mz0x1zLnm+eb15vft2BaeFostqi2uGVJsuRaplnutrxuhVo5WaVYVVpds0atna0l1rutu6cRp7lOk06rntZnw7Dxtsm2qbcZsOXYBtuutm22fWFnYhdnt8Wuw+6TvZN9un2N/T0HDYfZDqsdWh1+c7RyFDpWOt6azpzuP33F9JbpL2dYzxDP2DPjthPLKcRpnVOb00dnF2e5c4PziIuJS4LLLpc+Lpsbxt3IveRKdPVxXeF60vWdm7Obwu2o26/uNu5p7ofcn8w0nymeWTNz0MPIQ+BR5dE/C5+VMGvfrH5PQ0+BZ7XnIy9jL5FXrdewt6V3qvdh7xc+9j5yn+M+4zw33jLeWV/MN8C3yLfLT8Nvnl+F30N/I/9k/3r/0QCngCUBZwOJgUGBWwL7+Hp8Ib+OPzrbZfay2e1BjKC5QRVBj4KtguXBrSFoyOyQrSH355jOkc5pDoVQfujW0Adh5mGLw34MJ4WHhVeGP45wiFga0TGXNXfR3ENz30T6RJZE3ptnMU85ry1KNSo+qi5qPNo3ujS6P8YuZlnM1VidWElsSxw5LiquNm5svt/87fOH4p3iC+N7F5gvyF1weaHOwvSFpxapLhIsOpZATIhOOJTwQRAqqBaMJfITdyWOCnnCHcJnIi/RNtGI2ENcKh5O8kgqTXqS7JG8NXkkxTOlLOW5hCepkLxMDUzdmzqeFpp2IG0yPTq9MYOSkZBxQqohTZO2Z+pn5mZ2y6xlhbL+xW6Lty8elQfJa7OQrAVZLQq2QqboVFoo1yoHsmdlV2a/zYnKOZarnivN7cyzytuQN5zvn//tEsIS4ZK2pYZLVy0dWOa9rGo5sjxxedsK4xUFK4ZWBqw8uIq2Km3VT6vtV5eufr0mek1rgV7ByoLBtQFr6wtVCuWFfevc1+1dT1gvWd+1YfqGnRs+FYmKrhTbF5cVf9go3HjlG4dvyr+Z3JS0qavEuWTPZtJm6ebeLZ5bDpaql+aXDm4N2dq0Dd9WtO319kXbL5fNKNu7g7ZDuaO/PLi8ZafJzs07P1SkVPRU+lQ27tLdtWHX+G7R7ht7vPY07NXbW7z3/T7JvttVAVVN1WbVZftJ+7P3P66Jqun4lvttXa1ObXHtxwPSA/0HIw6217nU1R3SPVRSj9Yr60cOxx++/p3vdy0NNg1VjZzG4iNwRHnk6fcJ3/ceDTradox7rOEH0x92HWcdL2pCmvKaRptTmvtbYlu6T8w+0dbq3nr8R9sfD5w0PFl5SvNUyWna6YLTk2fyz4ydlZ19fi753GDborZ752PO32oPb++6EHTh0kX/i+c7vDvOXPK4dPKy2+UTV7hXmq86X23qdOo8/pPTT8e7nLuarrlca7nuer21e2b36RueN87d9L158Rb/1tWeOT3dvfN6b/fF9/XfFt1+cif9zsu72Xcn7q28T7xf9EDtQdlD3YfVP1v+3Njv3H9qwHeg89HcR/cGhYPP/pH1jw9DBY+Zj8uGDYbrnjg+OTniP3L96fynQ89kzyaeF/6i/suuFxYvfvjV69fO0ZjRoZfyl5O/bXyl/erA6xmv28bCxh6+yXgzMV70VvvtwXfcdx3vo98PT+R8IH8o/2j5sfVT0Kf7kxmTk/8EA5jz/GMzLdsAAAAgY0hSTQAAeiUAAICDAAD5/wAAgOkAAHUwAADqYAAAOpgAABdvkl/FRgAABb5JREFUeNrEl1tsVFUUhr+9z6VzoUgvFJqUCZUWoREEgaiJQaXcRKLBEDXGRF+MvoBBTUxjRRONxhiNQQ0m+KAPEhEjKBIQQ4pgo4lgKNSCIhCgUEVKC6VzZs5lbx/OaTvTmSm8uZLzcPZl/Xuvvda/1hK99a3cgDQBC4F7gflA7aj5HuAgsA/YD3RdT6F5nfk7gaeA1UDVGOumRd+jQC/wFfAp8EupDbLEeAJ4HdgBPFMU1A9A6WJ7q6I9OyIdiRsFrgW2Aq1AddFj+QpRmQRTjmWt6kjH1iJPUwA8GdgNrCipzguQqQoS6x8g/twicLzrPeeKSOfkUsBJ4DNgNgBag1doTu0p5NQqRNLGumc62vOLWoRA5Y7MjnQniwG3AEtD7QACUT0uBA5ywAUQKPTlQfRgFoTIB3UDRNJGJO3QD0ZkaYSRBzwfWJtrTnN+iuQHjxBbc284NtqRpCi8qesjp1WTeOdhYs8vRlaXo728m6+NsIaBnwbKh82pFDJVgawpx2q+hXjr/eGt82+Q/wRpF6OxhnjLMnADrNvqIG4h8g9cHmEhgZnAqtxZYZu4X3fgbu9A9acx56dIvLoCraIwkgJhm4i4HVrC8TCaaom1LEOd70dUxEm/8wPB6UtQZow+4ypgpgTuAyYWmFFrMh/+SHC4G31pAGNOHck3HoRAo8734x06g7vzaLi8cSLxlmWo070Y0yaS2dSOt/8EImYVM85E4D7RW9/6RcQ4haI0OlDE1zVjNNXi/3YWYQjs5hlorcFT4Ae433fB+Bj2HfU4m9rx9nQhymNjhdgW0VvfegJoKLnEV1BmYs5LUfb4AszpNaTf2IXf0Q2JMmKPzcNePZfMJ+0Ex//Ba/sTkSwLvb+0/GUCdcO/gUIXI4S+NDge5vQaBtZsQXVegJgFVzOk39yN9gLsJU1c3XgAtA7DLFcMGZp95DB1JqAAtOtjTp9EvGU5whR58Rl09UCyDO+nkwS/9yBidqjEEIikTfbzX7HumkZiXTPG3CmIWJR7dOgvfsc5nI0HIO2CIQHUcHYSGswFKcSEOF7bHyOOYUqC439j3d2A6r0WHkiMckTHRTtZtBfgd5wLyUOFFhRV4zBvTyGrxxGc6kUYI2lRAmhT4n7Xic74iPFx9BUnMpOB6nNQ3X0YM2sRhgxDaIhAPIWoq0RMGk9w4iJy8nj0QDakXIB+B/fbDtTZPoQ9HFrSBLqBBmFI9KBLdvPBkU1DkvFQZy5TvnIW9uq5uFsOoX0FAsSEOImWZag/L+Lu6gyJxshlYg2WiYjnhVb32OEUMZI5pw5ryQz01QyxJ+7AP3IB9/suZGUC66HZoBTu9qMYUytJv70HoQFrzJS5RUalSnHJ+pgzJxNb14yoTCLrKkKv7umn7MFZmPNSeNsOk3l3L9aiRsw760msX4FGlyoShmS/BNqAfwtu6ngYjTXEWpaju/swp1bht58i+Pk02S8P4XeeR11xcDa04R+9gDr+D6p3MKTX9Q9EWU0VA/0XaJPAMWDb6GRvNNQQe3Ex6txlZEMNzqb2kCLjFnJKBUZjDebcVMhQWR/nvb0EnedRlwcx500h/sJikLLYzbcBx4YeYhMwkJvsrYUNmLfVYUypIPPxAby9f4QgQoTKfAWuPxxyGJLM+20Eh84iLIm98lbkzVVoNy+jDURYw1XmQWAD8HKYnQy8vccRN8XwfjmN334ypMGxJAqvzEc/oi5eQzsuqrs/N4SIMA6OLm/fAhYASzElqucKmQ37QnIZDSpEWBYVA1eQ3fwrQoowrEYKhj0RRkHpMwg8CRwZ4ldsI/yKFHxGU21IjboQXJSZYBm5oEci3YPD9P1SxcLcbdeAb6LioLGYRYUh0X1ptOPh7TmGunAVjDFT0c6IJ/7O01OihUlEhdmzRWvrIedCg12yGbkEbAReG0pEN9LCpIFXotMWtjBSFH+CUG6ohRH/V9P23wB9nDzOL8MXpAAAAABJRU5ErkJggg==" alt="" />';}
				  } else if ( values[0] == "get_MODE" &&  values[1] == "Отопление" ) { //console.log(type + ": " + values[0] + "<-->"  + values[1] );
                  var element1=document.getElementById("get_work5");
				  var element2=document.getElementById("get_mode");
				  if(element1) {document.getElementById('get_mode').innerHTML = values[1] ;}
				  if(element1) {document.getElementById('get_work5').innerHTML = '<img src="data:image/png;base64,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" alt="" />';}
				  
				  } else if ( values[0] == "get_MODE" &&  values[1] == "Заморозка" ) { //console.log(type + ": " + values[0] + "<-->"  + values[1] );
                  var element1=document.getElementById("get_work5");
				  var element2=document.getElementById("get_mode");
				  if(element1) {document.getElementById('get_mode').innerHTML = values[1] ;}
				  if(element1) {document.getElementById('get_work5').innerHTML = '<img src="data:image/png;base64,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" alt="" />';}
				  } else if ( values[0] == "get_MODE" &&  values[1] == "Охлаждение" ) { //console.log(type + ": " + values[0] + "<-->"  + values[1] );
                  var element1=document.getElementById("get_work5");
				  var element2=document.getElementById("get_mode");
				  if(element1) {document.getElementById('get_mode').innerHTML = values[1] ;}
				  if(element1) {document.getElementById('get_work5').innerHTML = '<img src="data:image/png;base64,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" alt="" />';}
				  

				} else if ( values[0] == "get_uptime") {
                var element1=document.getElementById("get_uptime");
                var element2=document.getElementById("get_uptime2");
                if(element1) {document.getElementById("get_uptime").innerHTML =  values[1] ;} 
                if(element2) {document.getElementById("get_uptime2").innerHTML =  values[1] ;}                   
                } else if ( values[0] == "get_EEV") {
                var element1=document.getElementById("get_eev");
                var element2=document.getElementById("get_eev2");
                if(element1) {document.getElementById("get_eev").innerHTML =  values[1];} 
                if(element2) {document.getElementById("get_eev2").innerHTML =  values[1];}                   
                } else if ( values[0] == "set_EEV") {
                var element1=document.getElementById("get_eev");
                var element2=document.getElementById("get_eev2");
                if(element1) {document.getElementById("get_eev").innerHTML =  values[1];} 
                if(element2) {document.getElementById("get_eev2").innerHTML =  values[1];}                   
                } else if ( values[0] == "get_errcode" &&  values[1] == 0 ) {
                  document.getElementById("get_errcode").innerHTML = "OK";
                  document.getElementById("get_error").innerHTML = "";
                } else if (values[0] == "get_errcode" &&  values[1] < 0) {
                  document.getElementById("get_errcode").innerHTML = "Ошибка";
                } else if (values[0] == "test_Mail") {
                    setTimeout(loadParam('get_Message(MAIL_RET)'), 3000);
                }  else if (values[0] == "test_SMS") {
                    setTimeout(loadParam('get_Message(SMS_RET)'), 3000);
                } else if (values[0] == "set_SAVE") {
                  if (values[1] > 0) {
                      alert("Настройки сохранены в EEPROM, записано " + values[1] +" байт" );
                    } else {
                      alert("Ошибка записи в EEPROM, код ошибки:" + values[1] );
                    }
                } else if (values[0] == "RESET" || values[0] == "RESET_JOURNAL"|| values[0] == "set_updateNet") { // console.log(type + ": " + values[0] + "<-->"  + values[1] );
                      alert(values[1]); 
                } else if (values[0].toLowerCase() == "set_off" || values[0].toLowerCase() == "set_on") {
                  break;
                
				} else if (type == 'str') {
			var valueid = values[0].replace(/set_/g,"get_")./*replace(/\([0-9]*\)/g,"").*/replace(/\(/g,"-").replace(/\)/g,"").toLowerCase(); //.toLowerCase().replace(/set_/g,"get_").replace(/\([0-9]\)/g,"");
                var valuevar = values[1].toLowerCase().replace(/[^\w\d]/g,""); console.log("111111111111111111111111111111111valueid:"+valueid+" 2222222222222222222222222222222222valuevar:"+valuevar);
                if (valueid != null  && valueid != ""  && values[1] != null) {		
				var element=document.getElementById(valueid);
                  if(element) {document.getElementById(valueid).innerHTML=values[1];
				}
				}
				
				} else {
                  element=document.getElementById(values[0].toLowerCase()); if(element) {element.value = values[1]; element.innerHTML = values[1];}
                }
              }
            }
          }
        } // end: if (request.responseText != null)
      } // end: if (request.responseText != null)
    } else if( noretry != true) { console.log("request.status: "+request.status+" retry load..."); check_ready = 1; setTimeout(function() { loadParam(paramid); }, 4000); }
    autoheight(); // update height
  }  
  }
}

function calcacp()
{
  var a1=document.getElementById("a1").value;var a2=document.getElementById("a2").value;var p1=document.getElementById("p1").value;var p2=document.getElementById("p2").value;var k1=document.getElementById("k1");var k2=document.getElementById("k2");
  //console.log("a1:"+a1+" a2:"+a2+" p1:"+p1+" p2:"+p2);
  kk2 = (p2-p1)*100/(a2-a1);
  kk1 = p1-kk2*a1;
  k1.innerHTML = Math.abs(Math.round(kk1));
  k2.innerHTML = Math.abs(kk2.toFixed(3)); 
}

function dhcp(dcb)
{
  dhcpcheckbox = document.getElementById(dcb);
  if (dhcpcheckbox.checked)
  {
    //console.log("dhcp:enabled");
   document.getElementById('get_network-address').disabled = true;
   document.getElementById('get_network-subnet').disabled = true;
   document.getElementById('get_network-gateway').disabled = true;
   document.getElementById('get_network-dns').disabled = true;
   document.getElementById('get_network-address2').disabled = true;
   document.getElementById('get_network-subnet2').disabled = true;
   document.getElementById('get_network-gateway2').disabled = true;
   document.getElementById('get_network-dns2').disabled = true;  
  }
  else 
  {  //console.log("dhcp:disabled");
   document.getElementById('get_network-address').disabled = false;
   document.getElementById('get_network-subnet').disabled = false;
   document.getElementById('get_network-gateway').disabled = false;
   document.getElementById('get_network-dns').disabled = false;
   document.getElementById('get_network-address2').disabled = false;
   document.getElementById('get_network-subnet2').disabled = false;
   document.getElementById('get_network-gateway2').disabled = false;
   document.getElementById('get_network-dns2').disabled = false;
  }
}

function validip(valip) {
    var re = /\b(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\b/; 
    var valip = document.getElementById(valip).value;
    var valid = re.test(valip);
    if (!valid) alert ('Сетевые настройки введены неверно!');
    //document.getElementById('message').innerHTML = document.getElementById('message').innerHTML+'<br />'+output;
    return valid;
}

function validmac(valimac) {
    var re = /^[0-9a-f]{2}:[0-9a-f]{2}:[0-9a-f]{2}:[0-9a-f]{2}:[0-9a-f]{2}:[0-9a-f]{2}$/; 
    var valimac = document.getElementById(valimac).value;
    var valid = re.test(valimac);
    if (!valid) alert ('Аппаратный mac адрес введен неверно!');
    //document.getElementById('message').innerHTML = document.getElementById('message').innerHTML+'<br />'+output;
    return valid;
}

function swich(sw)
{
  swichid = document.getElementById(sw);
  if (swichid.checked)
  { 
    loadParam("set_ON");
  }
  else 
  {
    loadParam("set_OFF");
  }
}

function unique(arr) {
  var result = [];

  nextInput:
    for (var i = 0; i < arr.length; i++) {
      var str = arr[i]; // для каждого элемента
      for (var j = 0; j < result.length; j++) { // ищем, был ли он уже?
        if (result[j] == str) continue nextInput; // если да, то следующий
      }
      result.push(str);
    }

  return result;
}

function toggletable(elem) {
  el = document.getElementById(elem);
  el.style.display = (el.style.display == 'none') ? 'table' : 'none'
}


function upload(file) {
  var xhr = new XMLHttpRequest();
  xhr.upload.onprogress = function(event) { console.log(event.loaded + ' / ' + event.total); }
  xhr.onload = xhr.onerror = function() { if (this.status == 200) { console.log("success"); } else { console.log("error " + this.status); } };
  xhr.open("POST", urlcontrol, true);
  xhr.send(file);
  xhr.onreadystatechange = function()
  {
  if (this.readyState != 4) return;
    if (xhr.status == 200)
    { 
      if (xhr.responseText != null)
      {
          strResponse = xhr.responseText;
        alert(strResponse);
      }
    }
  } 
}

function autoheight() {
  var max_col_height = Math.max(
  document.body.scrollHeight, document.documentElement.scrollHeight,
  document.body.offsetHeight, document.documentElement.offsetHeight,
  document.body.clientHeight, document.documentElement.clientHeight
);  
  var columns = document.body.children;
  for (var i = columns.length - 1; i >= 0; i--) { // прокручиваем каждую колонку в цикле
    //console.log(columns[i].clientHeight);
    if( columns[i].offsetHeight > max_col_height ) {
      max_col_height = columns[i].offsetHeight; // устанавливаем новое значение максимальной высоты
    }
  }
  for (var i = columns.length - 1; i >= 0; i--) {
    //console.log(columns[i]+": "+max_col_height)
    columns[i].style.minHeight = max_col_height; // устанавливаем высоту каждой колонки равной максимальной
  }
}

function setKanalog() {
var k1=document.getElementById("k1").innerHTML;
var k2=document.getElementById("k2").innerHTML;
var sens=document.getElementById("get_listpress").selectedOptions[0].text; //console.log("sens:"+sens+" k1:"+k1+" k2:"+k2);
//if(sens="--") {return;}
zero=document.getElementById("get_zeropress-"+sens.toLowerCase());  if(zero)  {zero.value  = k1;  zero.innerHTML = k1;}
trans=document.getElementById("get_transpress-"+sens.toLowerCase());  if(trans)  {trans.value  = k2;  trans.innerHTML = k2;}
setParam('get_zeroPress('+sens+')');
setParam('get_transPress('+sens+')');
}

function calctpod(type) {
   var tout=document.getElementById("get_temp-tout").value;
   var cooltpod=document.getElementById("cooltpod");
   var heattpod=document.getElementById("heattpod");
   var tpidcool=document.getElementById("get_paramcoolhp-temp_pid").value;
   var tpidheat=document.getElementById("get_paramheathp-temp_pid").value;
   var kwcool=document.getElementById("get_paramcoolhp-k_weather").value;
   var kwheat=document.getElementById("get_paramheathp-k_weather").value;
   
   if (type == "heat")  { console.log("type:"+type+" kwheat:"+kwheat+" tpidheat:"+tpidheat);  heattpod.innerHTML = tpidheat - kwheat*tout; }
   else if (type == "cool") { console.log("type:"+type+" kwcool:"+kwcool+" tpidcool:"+tpidcool); cooltpod.innerHTML = tpidcool - kwcool*tout; }
}


function updateParam(paramids)
{ 
  setInterval(function() {loadParam(paramids)}, urlupdate); 
  loadParam(paramids);

}

function updatelog()
{ //console.log("updatelog");
  $('#textarea').load(urlcontrol+'/journal.txt', function() { document.getElementById("textarea").scrollTop = document.getElementById("textarea").scrollHeight; }); 
}