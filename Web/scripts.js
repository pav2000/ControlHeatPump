/* ver 0.951 beta */
var urlcontrol = 'http://77.50.254.24:25402'; // адрес и порт контроллера, если адрес сервера отличен от адреса контроллера (не рекомендуется)
//var urlcontrol = ''; //  автоопределение (если адрес сервера совпадает с адресом контроллера)
//var urlcontrol = 'http://192.168.0.199';
var urltimeout = 1800; // таймаут ожидание ответа от контроллера. Чем хуже интертнет, тем выше значения. Но не более времени обновления параметров
var urlupdate = 4010; // время обновления параметров в миллисекундах

function setParam(paramid, resultid) {
	// Замена set_Par(Var1) на set_par-var1 для получения значения 
	var elid = paramid.replace(/\(/g, "-").replace(/\)/g, "").toLowerCase();
	var rec = new RegExp('et_listChart');
	var rel = new RegExp('et_sensorListIP');
	var res = new RegExp('et_sensorListIP|et_freon|et_rule|et_listProfile|et_testMode|et_FC|et_EEV|et_modeHP');
	var ret = new RegExp('[(]SCHEDULER[)]');
	var recldr = new RegExp('Calendar');
	var elval, clear = true;
	var element;
	if(ret.test(paramid)) {
		var colls = document.getElementById("calendar").getElementsByClassName("clc");
		elval = "";
		for(var j = 0; j < colls.length; j++) {
			if(colls[j].innerHTML != "") elval += 1; else elval += 0;
			if(j % 24 == 23) elval += "/";
		}
	} else if(recldr.test(paramid)) { 
		var colls = document.getElementById("calendar").getElementsByClassName("clc");
		var fprof, lprof = -2, len = 0;
		elval = "";
		for(var j = 0; j < colls.length; j++) {
			var prof = colls[j].innerHTML == "" ? -1 : colls[j].innerHTML;
			if(prof != lprof) {
				elval += (((j / 24 | 0) << 5) | (j % 24)) + ";" + prof + ";";
				if(lprof == -2) {
					fprof = prof;
					flen = elval.length;
				}
				lprof = prof;
				len += 2;
			}
		}
		if(fprof == lprof && len > 2) {
			elval = elval.slice(flen);
			len -= 2;
		}
		elval = len + ";" + elval;
		clear = false;
	} else if((clear = elid.indexOf("=")==-1)) { // Не (x=n)
		if((element = document.getElementById(elid))) {
			if(element.getAttribute('type') == 'checkbox') {
				if(element.checked) elval = 1; else elval = 0;
			} else elval = element.value;
		} else { // not found, try resultid
			element = document.getElementById(resultid);
			elval = element.value;
		}
		if(typeof elval == 'string') elval = elval.replace(/[,=&]+/g, "");
	}
	if(res.test(paramid)) {
		var elsend = paramid.replace(/get_/g, "set_").replace(/\)/g, "") + "(" + elval + ")";
	} else if(rec.test(paramid)) {
		var elsend = paramid.replace(/get_listChart/g, "get_Chart(" + elval + ")");
		clear = false;
	} else {
		var elsend = paramid.replace(/get_/g, "set_");
		if(elsend.substr(-1) == ")") elsend = elsend.replace(/\)/g, "") + "=" + elval + ")"; else elsend += "=" + elval;  
	}
	if(rel.test(paramid)) elsend = elsend.replace(/\(/g, "=").replace(/\-/g, "(");
	if(!resultid) resultid = elid.replace(/set_/g, "get_");
	if(clear) {
		element = document.getElementById(resultid);
		if(element) {
			element.value = "";
			element.placeholder = "";
		}
	}
	loadParam(elsend, true, resultid);
}

function loadParam(paramid, noretry, resultdiv) {
	var check_ready = 1;
	var queue = 0;
	var req_stek = new Array();
	if(queue == 0) {
		req_stek.push(paramid);
	} else if(queue == 1) {
		req_stek.unshift(paramid);
		queue = 0;
	}

	if(check_ready == 1) {
		unique(req_stek);
		var oneparamid = req_stek.shift();
		check_ready = 0;

		var request = new XMLHttpRequest();
		var findz = new RegExp('/,/gi');
		//if (oneparamid.search(findz) != -1 )  { 
		var reqstr = oneparamid.replace(/,/g, '&');
		// } else { var reqstr = oneparamid; } 
		request.open("GET", urlcontrol + "/&" + reqstr + "&&", true);
		request.timeout = urltimeout;
		request.send(null);
		request.onreadystatechange = function() {
			if(this.readyState != 4) return;
			if(request.status == 200) {
				if(request.responseText != null) {
					strResponse = request.responseText.replace(/^&*/, '').replace(/&&*$/, '');
					var arr = strResponse.split('&');
					if(arr != null && arr != 0) {
						check_ready = 1; // ответ получен, можно слать следующий запрос.
						if(req_stek.length != 0) // если массив запросов не пуст - заправшиваем следующие значения.
						{
							queue = 1;
							setTimeout(function() {
								loadParam(req_stek.shift());
							}, 10); // запрашиваем следующую порцию.
						}
						for(var i = 0; i < arr.length; i++) {
							if(arr[i] != null && arr[i] != 0) {
								var reerr = new RegExp('^E');
								var rec = new RegExp('^CONST|get_infoFC|get_sysInfo|get_socketInfo|get_status');
								var rei = new RegExp('listFlow|listTemp|listInput|listRelay|sensorIP|get_numberIP|NUM_PROFILE|TASK_');
								var reo = new RegExp('^scan_');
								var rep = new RegExp('^get_present');
								var ret = new RegExp('[(]SCHEDULER[)]');
								var recldr = new RegExp('Calendar');
								var res = new RegExp('PING_TIME|et_listFlow|et_listPress|et_sensorListIP|et_freon|et_rule|et_testMode|get_listProfile|et_listChart|HP[(]RULE|HP[(]TARGET|SOCKET|RES_W5200|et_modeHP|TIME_CHART|SMS_SERVICE|et_optionHP[(]ADD_HEAT|et_SCHDLR[(]lstNames');
								var rev = new RegExp(/\([a-z0-9_]+\)/i);
								var reg = new RegExp('^get_Chart');
								var remintemp = new RegExp('^get_mintemp');
								var remaxtemp = new RegExp('^get_maxtemp');
								var retblval = new RegExp('et_modbus_');
								values = arr[i].split('=');
								var valueid = values[0].replace(/\(/g, "-").replace(/\)/g, "").toLowerCase().replace(/set_/g, "get_");
								var type, element;
								if(rec.test(values[0])) type = "const"; 
								else if(rei.test(values[0])) type = "table"; 
								else if(res.test(values[0])) type = "select"; // значения
								else if(reg.test(values[0])) type = "chart"; // график
								else if(ret.test(values[0])) type = "scheduler"; // расписание бойлера
								else if(reo.test(values[0])) type = "scan"; // ответ на сканирование
								else if(rep.test(values[0])) type = "present"; // наличие датчика в конфигурации
								else if(recldr.test(values[0])) type = "calendar"; // расписание
								else if(retblval.test(values[0])) type = "tableval"; // таблица значений
								else if(values[0].match(/^hide_/)) { // clear
									if(values[1] == 1) {
										var elements = document.getElementsByName(valueid);
										for(i = 0; i < elements.length; i++) elements[i].innerHTML = "";
									}
									continue;
								} else if(values[0].match(/^set_EEV/)) {
									if((element = document.getElementById("get_eev"))) element.value = values[1];
									if((element = document.getElementById("get_eev2"))) element.innerHTML = values[1];
								} else if(values[0].match(/^RELOAD/)) { 
									location.reload();
								} else {
									if((element = document.getElementById(valueid + "-ONOFF"))) { // Надпись
										element.innerHTML = values[1] == 1 ? "Вкл" : "Выкл";
									}
									element = document.getElementById(valueid);
									if(element && element.getAttribute('type') == 'checkbox') {
										var onoff = values[1] == 1;
										element.checked = onoff;
										if(valueid == "get_mqtt-use_thingspeak") {
											if((element=document.getElementById('get_mqtt-cop_mqtt'))) element.disabled = onoff;
											if((element=document.getElementById('get_mqtt-big_mqtt'))) element.disabled = onoff;
											if((element=document.getElementById('get_mqtt-fc_mqtt'))) element.disabled = onoff;
											if((element=document.getElementById('get_mqtt-sdm_mqtt'))) element.disabled = onoff;
											toggleclass('thingspeakon', onoff);
											toggleclass('thingspeakoff', !onoff);
										} 
										continue;
									} 
									type = rev.test(values[0]) ? "values" : "str";
								}
								if(type == 'scheduler') {
									var colls = document.getElementById("calendar").getElementsByClassName("clc");
									var cont1 = values[1].replace(/\//g, "");
									for(var j = 0; j < colls.length; j++) {
										colls[j].innerHTML = cont1.charAt(j) == 1 ? window.calendar_act_chr : "";
									}
								} else if(type == 'calendar') { // {WeekDay+Hour};{Profile|};...
									if(values[1] == "E33") alert("Ошибка: не верный номер расписания!");
									else if(values[1] == "E34") alert("Ошибка: нет места для календаря!");
									else {
										var colls = document.getElementById("calendar").getElementsByClassName("clc");
										var cal_pack = values[1].split(';');
										var cal_day = 0, cal_hour = 0;
										for(var j = 0; j < colls.length; j++) {
											if(cal_pack.length < 2) {
												colls[j].innerHTML = "";
												continue;
											}
											var found = 0;
											var dw = j / 24 | 0, h = j % 24;
											var ptr = 0;
											for(; ptr < cal_pack.length; ptr += 2) {
												var c_dw = cal_pack[ptr] >> 5, c_h = cal_pack[ptr] & 0x1F; 
												if(c_dw > dw || (c_dw == dw && c_h > h)) break;
												found = ptr + 1;
											}
											if(!found) found = cal_pack.length - 1;
											colls[j].innerHTML = cal_pack[found];
										}
									}
								} else if(type == 'chart') {
									if(values[0] != null && values[0] != 0 && values[1] != null && values[1] != 0) {
										title = values[0].replace(/get_Chart\(_/g, "").replace(/\)[0-9]?/g, "");
										var yizm = '';
										var ytooltip = '';
										var timeval = '';
										var height = 300;
										var visible = false;
										timeval = window.time_chart;
										timeval = Number(timeval.replace(/\D+/g, ""));
										var today = new Date();
										var regexpt = /^(T|O|d)/g;
										var regexpp = /^PE/g;
										var regexpe = /^pos/g;
										var regexpr = /^R/g;
										var regexpw = /^Pow|pow/g;
										if(regexpt.test(title)) { yizm = "Температура, °C"; ytooltip = " °C"; }
										if(regexpp.test(title)) { yizm = "Давление, BAR"; ytooltip = " BAR"; }
										if(regexpr.test(title)) { yizm = "Состояние реле"; ytooltip = ""; }
										if(regexpe.test(title)) { yizm = "Позиция, шаги"; ytooltip = " шаг"; }
										if(regexpw.test(title)) { yizm = "Мощность, кВт"; ytooltip = " kW"; }
										data = values[1].split(';');

										var dataSeries1 = [];
										//dataSeries1.pointInterval = timeval; 
										if(title == 'posEEV') {
											var poseev = [];
										}
										//dataSeries.pointStart = parseInt(start); // its important to parseInt !!!!
										for(var i = 0; i < data.length - 1; i++) {
											//today.setSeconds(today.getSeconds() + 10);  console.log(today.toLocaleTimeString());
											if(title == 'posEEV') {
												poseev.push([i, Number(data[i])]);
											} else {
												dataSeries1.push([i, Number(data[i])]); //data: [{ x: 1880, y: -0.4 }, { x: 2014, y: 0.52 }]
											}
										}

										if(title == 'posEEV') {
											window.poseev = poseev;
											dataSeries1 = poseev;
										}
										if(title == 'OVERHEAT') {
											dataSeries2 = window.poseev;
											visible = true;
										} else {
											var dataSeries2 = [];
										}
										//if (title == 'posEEV') {break;}

										$('#' + resultdiv).highcharts({
											title: {
												text: title,
												x: -15
											},
											chart: {
												type: 'line',
												zoomType: 'xy',
												height: height,
												animation: false,
												resetZoomButton: {
													position: {
														align: undefined,
														verticalAlign: "top",
														x: 20,
														y: -40
													},
													relativeTo: "plot"}
											},
											lang: {
												contextButtonTitle: "Меню графика",
												decimalPoint: ".",
												downloadJPEG: "Скачать JPEG картинку",
												downloadPDF: "Скачать PDF документ",
												downloadPNG: "Скачать PNG картинку",
												downloadSVG: "Скачать SVG векторную картинку",
												drillUpText: "Вернуться к {series.name}",
												loading: "Загрузка...",
												noData: "Нет информации для отображения",
												numericSymbolMagnitude: 1000,
												numericSymbols: ["k", "M", "G", "T", "P", "E"],
												printChart: "Распечатать график",
												resetZoom: "Сброс увеличения",
												resetZoomTitle: "Сброс увеличения к 1:1"
											},
											xAxis: {
												title: {
													text: "Время, шаг: x" + window.time_chart}},
											/*yAxis: [{ title: { text: yizm },labels: {align: 'left', x: 0, y: 0, format: '{value:.,0f}' }, plotLines: [{  value: 0,  width: 1,  color: '#808080'  }] },
							{ title: { text: 'Положение ЭРВ(шаги)' },labels: {align: 'right', x: 0, y: 0, format: '{value:.,0f}' }, plotLines: [{  value: 0,  width: 1,  color: '#808080'  }] },
							opposite: true],*/
											yAxis: [{ // Primary yAxis
												allowDecimals: false,
												labels: {
													format: '{value}',
													style: {
														color: Highcharts.getOptions().colors[0]}},
												title: {
													text: yizm,
													style: {
														color: Highcharts.getOptions().colors[0]}}
											}, { // Secondary yAxis
												allowDecimals: false,
												showEmpty: false,
												visible: visible,
												title: {
													text: 'Положение ЭРВ',
													style: {
														color: Highcharts.getOptions().colors[1]}},
												labels: {
													format: '{value} шагов',
													style: {
														color: Highcharts.getOptions().colors[1]}},
												opposite: true
											}],
											tooltip: {
												valueSuffix: ''
											},
											legend: {
												layout: 'vertical',
												align: 'right',
												verticalAlign: 'middle',
												borderWidth: 0
											},
											//plotOptions: { series: { dataGrouping: { enabled: false } } },
											plotOptions: {
												series: {
													label: {
														connectorAllowed: false},
										            animation: false,
													pointStart: 0
												}
											},
											series: [{
													yAxis: 0,
													name: title,
													tooltip: {
														valueDecimals: 2},
													states: {
														hover: {
															enabled: false}},
													showInLegend: false,
													turboThreshold: 0,
													data: dataSeries1,
													dashStyle: "Solid"
												},
												{
													yAxis: 1,
													name: 'Положение ЭРВ',
													tooltip: {
														valueDecimals: 2},
													states: {
														hover: {
															enabled: false}},
													showInLegend: false,
													turboThreshold: 0,
													data: dataSeries2,
													dashStyle: "Solid"
												}
											]

										});
									}
								} else if(type == 'scan') {
									if(values[0] != null && values[0] != 0 && values[1] != null && values[1] != 0) {
										var content = "<tr><td>" + values[1].replace(/\:/g, "</td><td>").replace(/(\;)/g, "</td></tr><tr><td>") + "</td></tr>";
										document.getElementById(values[0].toLowerCase()).innerHTML = content;
										content = values[1].replace(/[0-9]:.{1,10}:[-0-9\.]{1,5}:/g, "").replace(/;$/g, "");
										var cont2 = content.split(';');
										var elems = document.getElementById("scan_table").getElementsByTagName('select');
										for(var j = 0; j < elems.length; j++) {
											elems[j].options.length = 0
											elems[j].add(new Option("---", "", true, true), null);
											elems[j].add(new Option("reset", 0, false, false), null);
											for(var k = 0; k < cont2.length; k++) {
												elems[j].add(new Option(k + 1, k + 1, false, false), null);
											}
										}
									}
								} else if(type == 'select') {
									if(values[0] != null && values[0] != 0 && values[1] != null && values[1] != 0) {
										var idsel = values[0].toLowerCase().replace(/set_/g, "get_").replace(/\([0-9]\)/g, "").replace(/\(/g, "-").replace(/\)/g, "").replace(/_skip[1-9]$/,"");
										if(idsel == 'get_sensorlistip') idsel = valueid;
										if(idsel == "get_testmode") {
											var element2 = document.getElementById("get_testmode2");
											if(element2) {
												var cont1 = values[1].split(';');
												for(var n = 0, len = cont1.length; n < len; n++) {
													cont2 = cont1[n].split(':');
													if(cont2[1] == 1) {
														if(cont2[0] != "NORMAL") {
															document.getElementById("get_testmode2").className = "red";
														} else {
															document.getElementById("get_testmode2").className = "";
														}
														document.getElementById("get_testmode2").innerHTML = cont2[0];
													}
												}
											}
										} else if(idsel == "get_ruleeev") {
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
										} else if(idsel == "get_message-smtp_server") {
											loadParam("get_Message(SMTP_IP),get_Message(SMS_IP)");
										} else if(idsel == "get_message-sms_service") {
											loadParam("get_Message(SMS_NAMEP1),get_Message(SMS_NAMEP2),get_Message(SMS_IP)");
										} else if(idsel == "get_message-sms_service") {
											loadParam("get_Message(SMS_NAMEP1),get_Message(SMS_NAMEP2),get_Message(SMS_IP)");
										} else if(idsel == "get_listpress") {
											content = "";
											content2 = "";
											upsens = "";
											loadsens = "";
											var count = values[1].split(';');
											for(var j = 0; j < count.length - 1; j++) {
												input = count[j].toLowerCase();
												loadsens = loadsens + "get_zeroPress(" + count[j] + "),get_transPress(" + count[j] + "),get_maxPress(" + count[j] + "),get_minPress(" + count[j] + "),get_pinPress(" + count[j] + "),get_notePress(" + count[j] + "),get_testPress(" + count[j] + "),";
												upsens = upsens + "get_Press(" + count[j] + "),get_adcPress(" + count[j] + "),get_errcodePress(" + count[j] + "),";
												content = content + '<tr id="get_presentpress-' + input + '">';
												content = content + '<td>' + count[j] + '</td>';
												content = content + '<td id="get_notepress-' + input + '"></td>';
												content = content + '<td id="get_press-' + input + '">-</td>';
												content = content + '<td id="get_minpress-' + input + '">-</td>';
												content = content + '<td id="get_maxpress-' + input + '">-</td>';
												content = content + '<td nowrap><input id="get_zeropress-' + input + '" type="number" min="0" max="2048" step="1" value=""><input type="submit" value=">"  onclick="setParam(\'get_zeroPress(' + count[j] + ')\');"></td>';
												content = content + '<td nowrap><input id="get_transpress-' + input + '" type="number" min="0" max="4" step="0.001" value=""><input type="submit" value=">"  onclick="setParam(\'get_transPress(' + count[j] + ')\');"></td>';
												content = content + '<td nowrap><input id="get_testpress-' + input + '" type="number" min="-1" max="50" step="0.01" value=""><input type="submit" value=">"  onclick="setParam(\'get_testPress(' + count[j] + ')\');"></td>';
												content = content + '<td id="get_pinpress-' + input + '">-</td>';
												content = content + '<td id="get_adcpress-' + input + '">-</td>';
												content = content + '<td id="get_errcodepress-' + input + '">-</td>';
												content = content + '</tr>';
											}
											document.getElementById(idsel + "2").innerHTML = content;
											updateParam(upsens);
											loadParam(loadsens);
											values[1] = "--;" + values[1];
										}
										element = document.getElementById(idsel);
										if(element) {
											if(values[0].substr(-6, 5) == "_skip") {
												var j2 = Number(values[0].substr(-1)) - 1;
												for(var j = element.options.length - 1; j > j2; j--) element.options[j].remove();
											} else document.getElementById(idsel).innerHTML = "";
										}
										var element3 = document.getElementById(idsel);
										if(element3 && element3.tagName != "SPAN") {
											var cont1 = values[1].split(';');
											for(var k = 0, len = cont1.length - 1; k < len; k++) {
												cont2 = cont1[k].split(':');
												if(cont2[1] == 1) {
													selected = true;
													if(idsel == "get_optionhp-time_chart") {
														var time_chart = cont2[0];
														window.time_chart = time_chart;
													}
													if(idsel == "get_ruleeev") {
														if(k == 0 || k == 1) {
															document.getElementById("get_freoneev").disabled = true;
															document.getElementById("get_parameev-m_step").disabled = true;
															document.getElementById("get_parameev-m_step2").disabled = true;
														} else if(k == 2 || k == 3) {
															document.getElementById("get_parameev-m_step").disabled = true;
															document.getElementById("get_parameev-m_step2").disabled = true;
														} else if(k == 4) {
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
														} else if(k == 5) {
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
													} else if(idsel == "get_paramcoolhp-rule") {
														document.getElementById("get_paramcoolhp-target").disabled = false;
														document.getElementById("get_paramcoolhp-dtemp").disabled = false;
														document.getElementById("get_paramcoolhp-hp_pro").disabled = false;
														document.getElementById("get_paramcoolhp-hp_in").disabled = false;
														document.getElementById("get_paramcoolhp-hp_dif").disabled = false;
														document.getElementById("get_paramcoolhp-temp_pid").disabled = false;
														document.getElementById("get_paramcoolhp-weather").disabled = false;
														document.getElementById("get_paramcoolhp-k_weather").disabled = false;
														document.getElementById("get_paramcoolhp-hp_time").disabled = false;
														if(k == 2) {
															document.getElementById("get_paramcoolhp-target").disabled = true;
														} else if(k == 1) {
															document.getElementById("get_paramcoolhp-dtemp").disabled = true;
														} else if(k == 0) {
															document.getElementById("get_paramcoolhp-hp_time").disabled = true;
															document.getElementById("get_paramcoolhp-hp_pro").disabled = true;
															document.getElementById("get_paramcoolhp-hp_in").disabled = true;
															document.getElementById("get_paramcoolhp-hp_dif").disabled = true;
															document.getElementById("get_paramcoolhp-temp_pid").disabled = true;
															document.getElementById("get_paramcoolhp-weather").disabled = true;
															document.getElementById("get_paramcoolhp-k_weather").disabled = true;
														}
													} else if(idsel == "get_paramheathp-rule") {
														document.getElementById("get_paramheathp-target").disabled = false;
														//document.getElementById("get_paramheathp-dtemp").disabled = false;
														document.getElementById("get_paramheathp-hp_pro").disabled = false;
														document.getElementById("get_paramheathp-hp_in").disabled = false;
														document.getElementById("get_paramheathp-hp_dif").disabled = false;
														document.getElementById("get_paramheathp-temp_pid").disabled = false;
														document.getElementById("get_paramheathp-weather").disabled = false;
														document.getElementById("get_paramheathp-k_weather").disabled = false;
														document.getElementById("get_paramheathp-hp_time").disabled = false;
														if(k == 2) {
															document.getElementById("get_paramheathp-target").disabled = true;
														} else if(k == 1) { //document.getElementById("get_paramheathp-dtemp").disabled = true;
														} else if(k == 0) {
															document.getElementById("get_paramheathp-hp_time").disabled = true;
															document.getElementById("get_paramheathp-hp_pro").disabled = true;
															document.getElementById("get_paramheathp-hp_in").disabled = true;
															document.getElementById("get_paramheathp-hp_dif").disabled = true;
															document.getElementById("get_paramheathp-temp_pid").disabled = true;
															document.getElementById("get_paramheathp-weather").disabled = true;
															document.getElementById("get_paramheathp-k_weather").disabled = true;
														}
													} else if(idsel == "get_paramcoolhp-target") {
														if(k == 0) {
															document.getElementById("get_paramcoolhp-temp2").disabled = true;
															document.getElementById("get_paramcoolhp-temp1").disabled = false;
														} else if(k == 1) {
															document.getElementById("get_paramcoolhp-temp2").disabled = false;
															document.getElementById("get_paramcoolhp-temp1").disabled = true;
														}
													} else if(idsel == "get_paramheathp-target") {
														if(k == 0) {
															document.getElementById("get_paramheathp-temp2").disabled = true;
															document.getElementById("get_paramheathp-temp1").disabled = false;
														} else if(k == 1) {
															document.getElementById("get_paramheathp-temp2").disabled = false;
															document.getElementById("get_paramheathp-temp1").disabled = true;
														}
													}

												} else {
													selected = false;
												}
												if(idsel == "get_listchart") {
													var elems = document.getElementsByName("chrt_sel");
													for(var j = 0; j < elems.length; j++) {
														elems[j].add(new Option(cont2[0], "_" + cont2[0], false, selected), null);
													}
												} else {
													var opt = new Option(cont2[0], k, false, selected);
													if(cont2[2] == 0) opt.disabled = true;
													var element = document.getElementById(idsel);
													if(element) element.add(opt, null);
												}
											}
										}
									}
								} else if(type == 'const') {
									if(values[0] != null && values[0] != 0 && values[1] != null && values[1] != 0) {
										if(values[0] == 'get_status') {
											cont1 = values[1].replace(/\|/g, "</div><div>");
											var content = "<div>" + cont1 + "</div>";
										} else {
											cont1 = values[1].replace(/\|/g, "</td><td>");
											cont2 = cont1.replace(/(\;)/g, "</td></tr><tr><td>");
											var content = "<tr><td>" + cont2 + "</td></tr>";
										}
										document.getElementById(values[0].toLowerCase()).innerHTML = content;
									}
								} else if(type == 'table') {
									if(values[0] != null && values[0] != 0 && values[1] != null && values[1] != 0) {
										if(values[0] == 'get_numberIP') {
											content = "";
											content2 = "";
											upsens = "";
											loadsens = "";
											count = Number(values[1]);
											for(var j = 1; j < count + 1; j++) {
												upsens = upsens + "get_sensorIP(" + j + "),";
												loadsens = loadsens + "get_sensorListIP(" + j + "),get_sensorRuleIP(" + j + "),get_sensorUseIP(" + j + "),";
												content = content + '<tr id="get_sensorip-' + j + '"></tr>';
												content2 = content2 + '<tr><td><input type="checkbox" id="get_sensoruseip-' + j + '" onchange="setParam(\'get_sensorUseIP(' + j + ')\');" ></td>';
												content2 = content2 + '<td><input type="checkbox" id="get_sensorruleip-' + j + '" onchange="setParam(\'get_sensorRuleIP(' + j + ')\');" ></td>';
												content2 = content2 + '<td><select id="get_sensorlistip-' + j + '"  onchange="setParam(\'get_sensorListIP-' + j + '\',\'get_sensorlistip-' + j + '\');"></select></td></tr>';
											}
											document.getElementById(valueid).innerHTML = content;
											document.getElementById(valueid + "-inputs").innerHTML = content2;
											updateParam(upsens);
											loadParam(loadsens);

										} else if(values[0] == 'get_listRelay') {
											content = "";
											content2 = "";
											upsens = "";
											loadsens = "";
											var count = values[1].split(';');
											for(var j = 0; j < count.length - 1; j++) {
												if((relay = count[j].toLowerCase()) == "") continue;
												loadsens = loadsens + "get_pinRelay(" + count[j] + "),get_presentRelay(" + count[j] + "),get_noteRelay(" + count[j] + "),";
												upsens = upsens + "get_Relay(" + count[j] + "),";
												content = content + '<tr id="get_presentrelay-' + relay + '"><td>' + count[j] + '</td><td id="get_noterelay-' + relay + '"></td><td id="get_pinrelay-' + relay + '"></td><td><span id="get_relay-' + relay + '-ONOFF"></span><input type="checkbox" name="get_relay-' + relay + '" id="get_relay-' + relay + '" onchange="setParam(\'get_Relay(' + count[j] + ')\');"></td>';
												content = content + '</tr>';
											}
											document.getElementById(valueid).innerHTML = content;
											updateParam(upsens);
											loadParam(loadsens);

										} else if(values[0] == 'get_listInput') {
											content = "";
											content2 = "";
											upsens = "";
											loadsens = "";
											var count = values[1].split(';');
											for(var j = 0; j < count.length - 1; j++) {
												input = count[j].toLowerCase();
												loadsens = loadsens + "get_alarmInput(" + count[j] + "),get_errcodeInput(" + count[j] + "),get_typeInput(" + count[j] + "),get_pinInput(" + count[j] + "),get_Input(" + count[j] + "),get_noteInput(" + count[j] + "),get_testInput(" + count[j] + "),";
												upsens = upsens + "get_Input(" + count[j] + "),get_errcodeInput(" + count[j] + "),";
												content = content + '<tr><td>' + count[j] + '</td><td id="get_noteinput-' + input + '"></td><td id="get_input-' + input + '">-</td> <td nowrap><input id="get_alarminput-' + input + '" type="number" min="0" max="1" step="1" value=""><input type="submit" value=">" onclick="setParam(\'get_alarmInput(' + count[j] + ')\');"></td><td nowrap><input id="get_testinput-' + input + '" type="number" min="0" max="1" step="1" value=""><input type="submit" value=">"  onclick="setParam(\'get_testInput(' + count[j] + ')\');"></td><td id="get_pininput-' + input + '">-</td><td id="get_typeinput-' + input + '">-</td><td id="get_errcodeinput-' + input + '">-</td>';
												content = content + '</tr>';
											}
											document.getElementById(valueid).innerHTML = content;
											updateParam(upsens);
											loadParam(loadsens);

										} else if(values[0] == 'get_listTemp') {
											content = "";
											content2 = "";
											upsens = "";
											loadsens = "";
											var count = values[1].split(';');
											for(var j = 0; j < count.length - 1; j++) {
												input = count[j].toLowerCase();
												loadsens = loadsens + "get_errcodeTemp(" + count[j] + "),get_noteTemp(" + count[j] + "),get_testTemp(" + count[j] + "),get_minTemp(" + count[j] + "),get_maxTemp(" + count[j] + "),get_addressTemp(" + count[j] + "),";
												upsens = upsens + "get_fullTemp(" + count[j] + "),get_errTemp(" + count[j] + "),";
												content = content + '<tr id="get_presenttemp-' + input + '">';
												content = content + ' <td>' + count[j] + '</td>';
												content = content + ' <td id="get_notetemp-' + input + '"></td>';
												content = content + ' <td id="get_fulltemp-' + input + '">-</td>';
												content = content + ' <td id="get_mintemp-' + input + '">-</td>';
												content = content + ' <td id="get_maxtemp-' + input + '">-</td>';
												content = content + ' <td nowrap><input id="get_errtemp-' + input + '" type="number"  min="-5" max="5" step="0.1" value=""><input type="submit" value=">"  onclick="setParam(\'get_errTemp(' + count[j] + ')\');"></td>';
												content = content + ' <td nowrap><input id="get_testtemp-' + input + '" type="number" min="-5" max="5" step="0.1" value=""><input type="submit" value=">"  onclick="setParam(\'get_testTemp(' + count[j] + ')\');"></td>';
												content = content + ' <td id="get_addresstemp-' + input + '">-</td>';
												content = content + ' <td id="get_errcodetemp-' + input + '">-</td>';
												content = content + '</tr>';
											}
											document.getElementById(valueid).innerHTML = content;
											updateParam(upsens);
											loadParam(loadsens);

										} else if(values[0] == 'get_listFlow') {
											content = "";
											content2 = "";
											upsens = "";
											loadsens = "";
											var count = values[1].split(';');
											for(var j = 0; j < count.length - 1; j++) {
												input = count[j].toLowerCase();
												loadsens = loadsens + "get_noteFlow(" + count[j] + "),get_Flow(" + count[j] + "),get_minFlow(" + count[j] + "),get_kfFlow(" + count[j] + "),get_capacityFlow(" + count[j] + "),get_frFlow(" + count[j] + "),get_testFlow(" + count[j] + "),get_pinFlow(" + count[j] + "),get_errcodeFlow(" + count[j] + "),";
												upsens = upsens + "get_Flow(" + count[j] + "),get_frFlow(" + count[j] + "),get_errcodeFlow(" + count[j] + "),";
												content = content + '<tr>';
												content = content + '<td>' + count[j] + '</td>';
												content = content + '<td id="get_noteflow-' + input + '">-</td>';
												content = content + '<td id="get_flow-' + input + '">-</td>';
												content = content + '<td id="get_minflow-' + input + '">-</td>';
												content = content + '<td nowrap><input id="get_kfflow-' + input + '" type="number" min="0.001" max="655" step="1.000" style="max-width:70px;" value=""><input type="submit" value=">"  onclick="setParam(\'get_kfFlow(' + count[j] + ')\');"></td>';
												content = content + '<td nowrap><input id="get_capacityflow-' + input + '" type="number" min="0" max="65535" step="1" value=""><input type="submit" value=">"  onclick="setParam(\'get_capacityFlow(' + count[j] + ')\');"></td>';
												content = content + '<td id="get_frflow-' + input + '">-</td>';
												content = content + '<td nowrap><input id="get_testflow-' + input + '" type="number" min="0.0" max="1000" step="0.001" value=""><input type="submit" value=">"  onclick="setParam(\'get_testFlow(' + count[j] + ')\');"></td>';
												content = content + '<td id="get_pinflow-' + input + '">-</td>';
												content = content + '<td id="get_errcodeflow-' + input + '">-</td>';
												content = content + '</tr>';
											}
											document.getElementById(valueid).innerHTML = content;
											updateParam(upsens);
											loadParam(loadsens);
										} else if(values[0] == 'get_Profile(NUM_PROFILE)') {
											content = ""; content2 = ""; upsens = ""; loadsens = "";
											count = Number(values[1]);
											for(var j = 0; j < count; j++) {
												loadsens = loadsens + "infoProfile(" + j + "),";
												content = content + '<tr id="get_profile-' + j + '"><td>' + j + '</td><td id="infoprofile-' + j + '"></td>';
												content = content + '<td><input id="eraseprofile-' + j + '" type="submit" value="Стереть"  onclick=\'loadParam("eraseProfile(' + j + ')")\'> <input id="load-profile-' + j + '" type="submit" value="Загрузить"  onclick=\'loadParam("loadProfile(' + j + ')")\'></td></tr>';
											}
											document.getElementById(valueid).innerHTML = content;
											//document.getElementById(valueid + "-inputs").innerHTML = content2;
											loadParam(loadsens);
										} else {
											cont1 = values[1].replace(/\:$/g, "").replace(/\:/g, "</td><td>").replace(/\n/g, "</td></tr><tr><td>");
											count = valueid.replace(/[^\d;]/g, "");
											var content = '<td>' + cont1 + '</td>';
											var element = document.getElementById(valueid);
											if(element) element.innerHTML = content;
										}
									}
								} else if(type == 'values') {
									var valuevar = values[1].toLowerCase().replace(/[^\w\d]/g, "");
									if(valueid != null && valueid != "" && values[1] != null) {
										if(valueid == "get_message-sms_ret") {
											if(valuevar == "waitresponse") {
												setTimeout(loadParam('get_Message(SMS_RET)'), 3000);
												console.log("wait response...");
											} else alert(values[1]);
										}
										if(valueid == "get_message-mail_ret") {
											if(valuevar == "waitresponse") {
												setTimeout(loadParam('get_Message(MAIL_RET)'), 3000);
												console.log("wait response...");
											} else alert(values[1]);
										}
										if(values[1] == "Alarm") {
											document.getElementById(valueid.replace(/type/g, "alarm")).disabled = false;
										} else if(values[1] == "Work") {
											document.getElementById(valueid.replace(/type/g, "alarm")).disabled = true;
										}
										element3 = document.getElementById(valueid + "3");
										if(element3) {
											element3.value = values[1];
											element3.innerHTML = values[1];
										}
										element = document.getElementById(valueid);
										if(element) {
											if(element != document.activeElement) {
												element.value = values[1];
												element.innerHTML = values[1];
											}
										}
										if(reerr.test(values[1])) {
											element = document.getElementById(valueid);
											if(element) {
												if(element.getAttribute("type") == "submit") alert("Ошибка " + values[1]);
												else element.placeholder = values[1];
											}
										}
									}
									if(remintemp.test(valueid)) {
										document.getElementById(valueid.replace(/get_min/g, "get_test")).min = values[1];
									} else if(remaxtemp.test(valueid)) {
										document.getElementById(valueid.replace(/get_max/g, "get_test")).max = values[1];
									}

									if(valueid == "get_paramheathp-k_weather" || valueid == "get_paramheathp-temp_pid") {
										calctpod("heat");
									} else if(valueid == "get_paramcoolhp-k_weather" || valueid == "get_paramcoolhp-temp_pid") {
										calctpod("cool");
									}

								} else if(type == 'present') {
									if(valueid != null && values[1] != null && values[1] == 0) {
										element = document.getElementById(valueid);
										if(element) {
											element.className = "inactive";
										}
										if(values[0] == "get_presentRelay(RHEAT)" && values[1] == 0) {
											element = document.getElementById("get_optionhp-add_heat");
											if(element) {
												element.disabled = true;
											}
										}
										if(values[0] == "get_presentRelay(REVI)" && values[1] == 0) {
											element = document.getElementById("get_optionhp-temp_evi");
											if(element) {
												element.disabled = true;
											}
											element = document.getElementById("get_optionhp-temp_evi2");
											if(element) {
												element.disabled = true;
											}
										}
										var tableElem = document.getElementById(valueid);
										if(tableElem) {
											var elements = tableElem.getElementsByTagName('select');
											if(elements[0]) {
												elements[0].disabled = true;
											}
										}
									}
								} else if(type == 'tableval') {
									var element2 = document.getElementById(valueid.replace(/val/, "err"));
									if(values[1].match(/^E-?\d/)) {
										if(element2) element2.innerHTML = values[1]; 
									} else {
										if(element2) element2.innerHTML = "OK";
										if((element = document.getElementById(valueid))) {
											element.value = values[1];
											element2 = document.getElementById(valueid.replace(/val/, "hex"));
											if(element2) element2.value = "0x" + Number(values[1]).toString(16).toUpperCase();
										}
									}
								} else if(values[0] == "get_WORK") {
									element = document.getElementById(values[0].toLowerCase());
									var onoff = values[1] == "ON"; // "OFF"
									if(element) element.innerHTML = onoff ? "ВКЛ" : "ВЫКЛ";   
									element = document.getElementById("onoffswitch");
									if(element) element.checked = onoff;
									if((element=document.getElementById('get_zeroEEV'))) element.disabled = onoff;
									if((element=document.getElementById('get_testmode'))) element.disabled = onoff;
									if((element=document.getElementById('get_listprofile'))) element.disabled = onoff;
									if((element=document.getElementById('load-profile-0'))) element.disabled = onoff;
									if((element=document.getElementById('load-profile-1'))) element.disabled = onoff;
									if((element=document.getElementById('load-profile-2'))) element.disabled = onoff;
									if((element=document.getElementById('load-profile-3'))) element.disabled = onoff;
									if((element=document.getElementById('load-profile-4'))) element.disabled = onoff;
									if((element=document.getElementById('load-profile-5'))) element.disabled = onoff;
									if((element=document.getElementById('load-profile-6'))) element.disabled = onoff;
									if((element=document.getElementById('load-profile-7'))) element.disabled = onoff;
									if((element=document.getElementById('get_modehp'))) element.disabled = onoff;
									if((element=document.getElementById('scan'))) element.disabled = onoff;
									element = document.getElementById('manual_override'); if(element && element.checked) onoff = false;
									if((element=document.getElementById('get_relay-rcomp'))) element.disabled = onoff;
									if((element=document.getElementById('get_relay-rpumpi'))) element.disabled = onoff;
									if((element=document.getElementById('get_relay-rpumpo'))) element.disabled = onoff;
									if((element=document.getElementById('get_relay-rboiler'))) element.disabled = onoff;
									if((element=document.getElementById('get_relay-rheat'))) element.disabled = onoff;
									if((element=document.getElementById('get_relay-rtrv'))) element.disabled = onoff;
									if((element=document.getElementById('get_relay-rfan1'))) element.disabled = onoff;
									if((element=document.getElementById('get_relay-rfan2'))) element.disabled = onoff;
									if((element=document.getElementById('get_relay-r3way'))) element.disabled = onoff;
									if((element=document.getElementById('get_relay-revi'))) element.disabled = onoff;
									if((element=document.getElementById('get_relay-rpumpb'))) element.disabled = onoff;
									if((element=document.getElementById('get_eev'))) element.disabled = onoff;
									if((element=document.getElementById('get_eev3'))) element.disabled = onoff;
									if((element=document.getElementById('get_paramfc-on_off'))) element.disabled = onoff;
								} else if(values[0] == "get_uptime") {
									if((element = document.getElementById("get_uptime"))) element.innerHTML = values[1];
									if((element = document.getElementById("get_uptime2"))) element.innerHTML = values[1];
								} else if(values[0] == "get_errcode" && values[1] == 0) {
									document.getElementById("get_errcode").innerHTML = "OK";
									document.getElementById("get_error").innerHTML = "";
								} else if(values[0] == "get_errcode" && values[1] < 0) {
									document.getElementById("get_errcode").innerHTML = "Ошибка";
								} else if(values[0] == "test_Mail") {
									setTimeout(loadParam('get_Message(MAIL_RET)'), 3000);
								} else if(values[0] == "test_SMS") {
									setTimeout(loadParam('get_Message(SMS_RET)'), 3000);
								} else if(values[0].match(/^set_SAVE/)) { 
									if(values[1] >= 0) {
										if(values[0].match(/SCHDLR$/)) { 
											alert("Настройки расписаний сохранены в EEPROM!");
										} else {
											alert("Настройки сохранены в EEPROM, записано " + values[1] + " байт");
										}
									} else alert("Ошибка записи в EEPROM, код ошибки:" + values[1]);
								} else if(values[0] == "RESET" || values[0] == "RESET_JOURNAL" || values[0] == "set_updateNet") {
									alert(values[1]);
								} else if(values[0].toLowerCase() == "set_off" || values[0].toLowerCase() == "set_on") {
									break;
								} else {
									if((element = document.getElementById(values[0].toLowerCase()))) {
										element.value = values[1];
										element.innerHTML = values[1];
									}
									if((element = document.getElementById(values[0].toLowerCase() + "2"))) {
										element.value = values[1];
										element.innerHTML = values[1];
									}
								}
							}
						}
						if(typeof window["loadParam_after"] == 'function') window["loadParam_after"](paramid);
					} // end: if (request.responseText != null)
				} // end: if (request.responseText != null)
			} else if(noretry != true) {
				console.log("request.status: " + request.status + " retry load...");
				check_ready = 1;
				setTimeout(function() {
					loadParam(paramid);
				}, 4000);
			}
			autoheight(); // update height
		}
	}
}

function calcacp() {
	var a1 = document.getElementById("a1").value;
	var a2 = document.getElementById("a2").value;
	var p1 = document.getElementById("p1").value;
	var p2 = document.getElementById("p2").value;
	var k1 = document.getElementById("k1");
	var k2 = document.getElementById("k2");
	kk2 = (p2 - p1) * 100 / (a2 - a1);
	kk1 = p1 - kk2 * a1;
	k1.innerHTML = Math.abs(Math.round(kk1));
	k2.innerHTML = Math.abs(kk2.toFixed(3));
}

function dhcp(dcb) {
	dhcpcheckbox = document.getElementById(dcb);
	if(dhcpcheckbox.checked) {
		//console.log("dhcp:enabled");
		document.getElementById('get_network-address').disabled = true;
		document.getElementById('get_network-subnet').disabled = true;
		document.getElementById('get_network-gateway').disabled = true;
		document.getElementById('get_network-dns').disabled = true;
		document.getElementById('get_network-address2').disabled = true;
		document.getElementById('get_network-subnet2').disabled = true;
		document.getElementById('get_network-gateway2').disabled = true;
		document.getElementById('get_network-dns2').disabled = true;
	} else { //console.log("dhcp:disabled");
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
	if(!valid) alert('Сетевые настройки введены неверно!');
	//document.getElementById('message').innerHTML = document.getElementById('message').innerHTML+'<br />'+output;
	return valid;
}

function validmac(valimac) {
	var re = /^[0-9a-f]{2}:[0-9a-f]{2}:[0-9a-f]{2}:[0-9a-f]{2}:[0-9a-f]{2}:[0-9a-f]{2}$/;
	var valimac = document.getElementById(valimac).value;
	var valid = re.test(valimac);
	if(!valid) alert('Аппаратный mac адрес введен неверно!');
	//document.getElementById('message').innerHTML = document.getElementById('message').innerHTML+'<br />'+output;
	return valid;
}

function swich(sw) {
	swichid = document.getElementById(sw);
	if(swichid.checked) {
		loadParam("set_ON");
	} else {
		loadParam("set_OFF");
	}
}

function unique(arr) {
	var result = [];

	nextInput:
		for(var i = 0; i < arr.length; i++) {
			var str = arr[i]; // для каждого элемента
			for(var j = 0; j < result.length; j++) { // ищем, был ли он уже?
				if(result[j] == str) continue nextInput; // если да, то следующий
			}
			result.push(str);
		}

	return result;
}

function toggletable(elem) {
	el = document.getElementById(elem);
	el.style.display = (el.style.display == 'none') ? 'table' : 'none'
}

function toggleclass(elem, onoff) {
	var elems = document.getElementsByClassName(elem);
	for(var i = 0; i < elems.length; i++) {
		el = elems[i];
		if(onoff) el.style.display = 'block';
		if(!onoff) el.style.display = 'none';
	}
}

function upload(file) {
	var xhr = new XMLHttpRequest();
	xhr.upload.onprogress = function(event) {
		console.log(event.loaded + ' / ' + event.total);
	}
	xhr.onload = xhr.onerror = function() {
		if(this.status == 200) {
			console.log("success");
		} else {
			console.log("error " + this.status);
		}
	};
	xhr.open("POST", urlcontrol, true);
	xhr.send(file);
	xhr.onreadystatechange = function() {
		if(this.readyState != 4) return;
		if(xhr.status == 200) {
			if(xhr.responseText != null) {
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
	for(var i = columns.length - 1; i >= 0; i--) { // прокручиваем каждую колонку в цикле
		//console.log(columns[i].clientHeight);
		if(columns[i].offsetHeight > max_col_height) {
			max_col_height = columns[i].offsetHeight; // устанавливаем новое значение максимальной высоты
		}
	}
	for(var i = columns.length - 1; i >= 0; i--) {
		//console.log(columns[i]+": "+max_col_height)
		columns[i].style.minHeight = max_col_height; // устанавливаем высоту каждой колонки равной максимальной
	}
}

function setKanalog() {
	var k1 = document.getElementById("k1").innerHTML;
	var k2 = document.getElementById("k2").innerHTML;
	var sens = document.getElementById("get_listpress").selectedOptions[0].text;
	//if(sens="--") {return;}
	zero = document.getElementById("get_zeropress-" + sens.toLowerCase());
	if(zero) {
		zero.value = k1;
		zero.innerHTML = k1;
	}
	trans = document.getElementById("get_transpress-" + sens.toLowerCase());
	if(trans) {
		trans.value = k2;
		trans.innerHTML = k2;
	}
	setParam('get_zeroPress(' + sens + ')');
	setParam('get_transPress(' + sens + ')');
}

function calctpod(type) {
	var tout = document.getElementById("get_temp-tout").value;
	var cooltpod = document.getElementById("cooltpod");
	var heattpod = document.getElementById("heattpod");
	var tpidcool = document.getElementById("get_paramcoolhp-temp_pid").value;
	var tpidheat = document.getElementById("get_paramheathp-temp_pid").value;
	var kwcool = document.getElementById("get_paramcoolhp-k_weather").value;
	var kwheat = document.getElementById("get_paramheathp-k_weather").value;

	if(type == "heat") { 
		heattpod.innerHTML = tpidheat - kwheat * tout;
	} else if(type == "cool") {
		cooltpod.innerHTML = tpidcool - kwcool * tout;
	}
}

function updateParam(paramids) {
	setInterval(function() {
		loadParam(paramids)
	}, urlupdate);
	loadParam(paramids);
}

function updatelog() {
	$('#textarea').load(urlcontrol + '/journal.txt', function() {
		document.getElementById("textarea").scrollTop = document.getElementById("textarea").scrollHeight;
	});

}

function getCookie(name) {
	var matches = document.cookie.match(new RegExp("(?:^|; )" + name.replace(/([\.$?*|{}\(\)\[\]\\\/\+^])/g, '\\$1') + "=([^;]*)"));
	return matches ? decodeURIComponent(matches[1]) : undefined;
}
function setCookie(name, value) {
	document.cookie = name + '="' + escape(value) + '";expires=' + (new Date(2019, 1, 1)).toUTCString();
}