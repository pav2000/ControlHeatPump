// Copyright (c) 2016-2019 by Pavel Panfilov <firstlast2007@gmail.com> skype pav2000pav  
// &                       by Vadim Kulakov vad7@yahoo.com, vad711
var VER_WEB = "1.057";
var urlcontrol = ''; //  автоопределение (если адрес сервера совпадает с адресом контроллера)
// адрес и порт контроллера, если адрес сервера отличен от адреса контроллера (не рекомендуется)
//var urlcontrol = 'http://192.168.0.199';
//var urlcontrol = 'http://192.168.0.7';
//var urlcontrol = 'http://77.50.254.24:25402';
var urltimeout = 1800; // таймаут ожидание ответа от контроллера. Чем хуже интертнет, тем выше значения. Но не более времени обновления параметров
var urlupdate = 4000; // время обновления параметров в миллисекундах

function setParam(paramid, resultid) {
	// Замена set_Par(Var1) на set_par-var1 для получения значения 
	var elid = paramid.replace("(", "-").replace(")", "");
	var rec = new RegExp('_listChart.?');
	var res = new RegExp('_modeHP|_listProf|_testMode|_slIP');
	var elval, clear = true, equate = true;
	var element;
	if(paramid.indexOf("(SCHEDULER)")!=-1) {
		var colls = document.getElementById("calendar").getElementsByClassName("clc");
		elval = "";
		for(var j = 0; j < colls.length; j++) {
			if(colls[j].innerHTML != "") elval += 1; else elval += 0;
			if(j % 24 == 23) elval += "/";
		}
	} else if(paramid.indexOf("(Calendar")!=-1) { 
		var colls = document.getElementById("calendar").getElementsByClassName("clc");
		var fprof, lprof = -255, len = 0;
		elval = "";
		for(var j = 0; j < colls.length; j++) {
			var prof = colls[j].innerHTML;
			if(prof == "") prof = "0";
			else if(prof[0] == '+') prof = Number(prof.substring(1)) * 10;
			else if(prof[0] == '-') prof = 256 + Number(prof) * 10;
			else prof = Number(prof) + 0x80; 
			if(prof != lprof) {
				elval += (((j / 24 | 0) << 5) | (j % 24)) + ";" + prof + ";";
				if(lprof == -255) {
					fprof = prof;
					flen = elval.length;
				}
				lprof = prof;
				if((len += 2) == 254) break;
			}
		}
		if(fprof == lprof && len > 2) {
			elval = elval.slice(flen);
			len -= 2;
		}
		elval = len + ";" + elval;
		clear = false;
	} else if((clear = equate = elid.indexOf("=")==-1)) { // Не (x=n)
		if((element = document.getElementById(elid.toLowerCase()))) {
			if(element.getAttribute('type') == 'checkbox') {
				if(element.checked) elval = 1; else elval = 0;
			} else elval = element.value;
		} else { // not found, try resultid
			element = document.getElementById(resultid);
			elval = element.value;
		}
		//if(typeof elval == 'string') elval = elval.replace(/[,=&]+/g, "");
	}
	if(res.test(paramid)) {
		if(paramid.indexOf("Prof")!=-1) {
			elval = element.options[elval].innerHTML;
			elval = Number(elval.substr(0, elval.indexOf('.'))) - 1; 
		}
		var elsend = paramid.replace("get_", "set_").replace(")", "") + "(" + elval + ")";
	} else if(rec.test(paramid)) {
		var elsend = paramid.replace("get_listChart", "get_Chart(" + elval + ")");
		clear = false;
	} else {
		var elsend = paramid.replace("get_", "set_");
		if(equate) {
			if(elsend.substr(-1) == ")") elsend = elsend.replace(")", "") + "=" + elval + ")"; else elsend += "=" + elval;
		}
	}
	if(/[(]DSD\d/.test(paramid)) clear = false;
	if(/et_slIP/.test(paramid)) elsend = elsend.replace("(", "=").replace("-", "(");
	if(!resultid) resultid = elid.replace("set_", "get_").toLowerCase();
	if(clear) {
		element = document.getElementById(resultid);
		if(element) {
			element.value = "";
			element.placeholder = "";
		}
	}
	loadParam(encodeURIComponent(elsend), true, resultid);
}

var NeedLogin = sessionStorage.getItem("NeedLogin");
var LPString = sessionStorage.getItem("LPString");

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
		var reqstr = urlcontrol + "/&" + oneparamid.replace(/,/g, '&') + "&&";
		if(NeedLogin) {
			if(NeedLogin != 2) {
				LPString = "!Z" + window.btoa(prompt("Login:") + ":" + prompt("Password:"));
				NeedLogin = 2;
				sessionStorage.setItem("NeedLogin", NeedLogin);
				sessionStorage.setItem("LPString", LPString);
			}
			reqstr += LPString;
		} 
		request.open("GET", reqstr, true);
		request.timeout = urltimeout;
		request.send(null);
		request.onreadystatechange = function() {
			if(this.readyState != 4) return;
			if(request.status == 200) {
				if(request.responseText != null) {
					var arr = request.responseText.replace(/^\x7f+/, '').replace(/\x7f\x7f*$/, '').split('\x7f');
					if(arr != null && arr != 0) {
						check_ready = 1; // ответ получен, можно слать следующий запрос.
						if(req_stek.length != 0) // если массив запросов не пуст - заправшиваем следующие значения.
						{
							queue = 1;
							setTimeout(function() {	loadParam(req_stek.shift()); }, 10); // запрашиваем следующую порцию.
						}
						for(var i = 0; i < arr.length; i++) {
							if(arr[i] != null && arr[i] != 0) {
								values = arr[i].split('=');
								var valueid = values[0].replace("(", "-").replace(")", "").replace("set_", "get_").toLowerCase();
								var type, element;
								if(/get_status|get_pFC[(]INFO|get_sys|^CONST|get_socketInfo/.test(values[0])) type = "const"; 
								else if(/_list|et_modeHP|[(]RULE|et_testMode|[(]TARGET|NSL|SMS_SERVICE|et_slIP/.test(values[0])) type = "select"; // значения
								else if(/Prof[(]NUM|get_tbl|listRelay|sensorIP|get_numberIP|TASK_/.test(values[0])) type = "table"; 
								else if(values[0].indexOf("get_is")==0) type = "is"; // наличие датчика в конфигурации
								else if(values[0].indexOf("scan_")==0) type = "scan"; // ответ на сканирование
								else if(values[0].indexOf("hide_")==0) { // clear
									if(values[1] == 1) {
										var elements = document.getElementsByName(valueid);
										for(var j = 0; j < elements.length; j++) elements[j].innerHTML = "";
									} else {
										var elements = document.getElementsByName(valueid);
										for(var j = 0; j < elements.length; j++) elements[j].innerHTML = values[1];
									}
									continue;
								} else if(values[0].indexOf("get_Chart")==0) type = "chart"; // график
								else if(values[0].indexOf("(SCHEDULER)")!=-1) type = "sch"; // расписание бойлера
								else if(values[0].indexOf("(Calendar")!=-1) type = "cld"; // расписание
								else if(values[0].indexOf("et_modbus_")==1) type = "tbv"; // таблица значений
								else if(values[0].indexOf("set_pEEV(POS")==0) {
									var s = "get_peev-pos";
									if(values[0].substr(-1) == 'p') s += "p";  
									if((element = document.getElementById(s))) element.value = values[1];
									if((element = document.getElementById(s+"2"))) element.innerHTML = values[1];
								} else if(values[0].indexOf("RELOAD")==0) { 
									location.reload();
								} else if(values[0].indexOf("(DSD")!=-1) {
									loadParam("get_tblPDS");
								} else {
									if((element = document.getElementById(valueid + "-ONOFF"))) { // Надпись
										element.innerHTML = values[1] == 1 ? "Вкл" : "Выкл";
									}
									if(valueid == "get_schdlr-on") {
										if((element = document.getElementById("SCH-PR"))) element.innerHTML = values[1] == '1' ? "РАСПИСАНИЕ" : "ПРОФИЛЬ";
									}
									element = document.getElementById(valueid);
									if(element && element.getAttribute('type') == 'checkbox') {
										var onoff = values[1] == 1;
										element.checked = onoff;
										var elements = document.getElementsByName(valueid + "-hide");
										for(var j = 0; j < elements.length; j++) elements[j].style = "display:" + (onoff ? "inline" : "none");
										var elements = document.getElementsByName(valueid + "-unhide");
										for(var j = 0; j < elements.length; j++) elements[j].style = "display:" + (!onoff ? "inline" : "none");
										continue;
									} 
									type = /\([a-z0-9_]+\)/i.test(values[0]) ? "values" : "str";
								}
								if(type == 'sch') {
									var colls = document.getElementById("calendar").getElementsByClassName("clc");
									var cont1 = values[1].replace(/\//g, "");
									for(var j = 0; j < colls.length; j++) {
										colls[j].innerHTML = cont1.charAt(j) == 1 ? window.calendar_act_chr : "";
									}
								} else if(type == 'cld') { // {WeekDay+Hour};{Profile|};...
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
											var v = cal_pack[found];
											if(v >= 0x80 && v <= 0x9B) {
												v ^= 0x80;
												colls[j].style = "color:yellow";
											} else {
												if(v >= 0x80) v -= 256;
												v = v / 10; 
												if(v > 0) v = '+' + v;
												colls[j].style = "color:red";
											}
											colls[j].innerHTML = v ? v : "";
										}
									}
								} else if(type == 'chart') {
									if(values[0]) {
										if(!window.time_chart) { console.log("Chart was not intialized!"); continue; }
										createChart(values, resultdiv);
									}
								} else if(type == 'scan') {
									if(valueid == "get_message-scan_sms") {
										if(values[1] == "wait response") {
											setTimeout(loadParam('get_Message(scan_SMS)'), 3000);
										} else alert(values[1]);
									} else if(valueid == "get_message-scan_mail") {
										if(values[1] == "wait response") {
											setTimeout(loadParam('get_Message(scan_MAIL)'), 3000);
										} else alert(values[1]);
									} else if(values[0] != null && values[0] != 0 && values[1] != null && values[1] != 0) {
										var content = "<tr><td>" + values[1].replace(/\:/g, "</td><td>").replace(/(\;)/g, "</td></tr><tr><td>") + "</td></tr>";
										document.getElementById(values[0].toLowerCase()).innerHTML = content;
										content = values[1].replace(/:[^;]+/g, "").replace(/;$/g, "");
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
									if(values[0] != null && values[0] != 0 && values[1] != null) {
										var idsel = values[0].replace("set_", "get_").toLowerCase().replace(/\([0-9]\)/g, "").replace("(", "-").replace(")", "").replace(/_skip[1-9]$/,"");
										if(idsel.substr(-1, 1) == '_') {
											idsel = idsel.substring(0, idsel.length - 1);
											element = document.getElementById(idsel);
											if(element) {
												var n = (Number(values[1]) + 1).toString() + '.';
												for(var j = 0; j < element.options.length; j++) if(n == element.options[j].innerText.substr(0, n.length)) { element.options[j].selected = true; break; }
											}
											continue;
										}
										if(idsel == 'get_slip') idsel = valueid;
										else if(idsel == "get_testmode") {
											var element2 = document.getElementById("get_testmode2");
											if(element2) {
												var cont1 = values[1].split(';');
												for(var n = 0, len = cont1.length; n < len; n++) {
													var cont2 = cont1[n].split(':');
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
										} else if(idsel == "get_listpress") {
											content = "";
											content2 = "";
											upsens = "";
											loadsens = "";
											var count = values[1].split(';');
											for(var j = 0; j < count.length - 1; j++) {
												var P = count[j];
												loadsens += "get_zeroPress(" +P+ "),get_transPress(" +P+ "),get_maxPress(" +P+ "),get_minPress(" +P+ "),get_pinPress(" +P+ "),get_nPress(" +P+ "),get_testPress(" +P+ "),";
												upsens += "get_Press(" +P+ "),get_adcPress(" +P+ "),get_ePress(" +P+ "),";
												P = P.toLowerCase();
												content += '<tr id="get_ispress-' +P+ '">';
												content += '<td>' +count[j]+ '</td>';
												content += '<td id="get_npress-' +P+ '"></td>';
												content += '<td id="get_press-' +P+ '" nowrap>-</td>';
												content += '<td nowrap><input id="get_minpress-' +P+ '" type="number" min="-1" max="50" step="0.01"><input type="submit" value=">" onclick="setParam(\'get_minPress(' +count[j]+ ')\');"></td>';
												content += '<td nowrap><input id="get_maxpress-' +P+ '" type="number" min="-1" max="50" step="0.01"><input type="submit" value=">" onclick="setParam(\'get_maxPress(' +count[j]+ ')\');"></td>';
												content += '<td nowrap><input id="get_zeropress-' +P+ '" type="number" min="0" max="2048" step="1"><input type="submit" value=">" onclick="setParam(\'get_zeroPress(' +count[j]+ ')\');"></td>';
												content += '<td nowrap><input id="get_transpress-' +P+ '" type="number" min="0" max="4" step="0.001"><input type="submit" value=">" onclick="setParam(\'get_transPress(' +count[j]+ ')\');"></td>';
												content += '<td nowrap><input id="get_testpress-' +P+ '" type="number" min="-1" max="50" step="0.01"><input type="submit" value=">" onclick="setParam(\'get_testPress(' +count[j]+ ')\');"></td>';
												content += '<td id="get_pinpress-' +P+ '">-</td>';
												content += '<td id="get_adcpress-' +P+ '">-</td>';
												content += '<td id="get_epress-' +P+ '">-</td>';
												content += '</tr>';
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
											} else if(/^\d+$/.test(values[1])) {
												element.options[values[1]].selected = true;
											} else element.innerHTML = "";
											var cont1 = values[1].split(';');
											if(element.tagName != "SPAN") {
												for(var k = 0; k < cont1.length - 1; k++) {
													var cont2 = cont1[k].split(':');
													if(cont2[1] == 1) {
														selected = true;
														if(idsel == "get_cool-rule") {
															var onoff = k == 0;
															document.getElementById("get_cool-target").disabled = k == 2;
															//document.getElementById("get_cool-dtemp").disabled = k == 1;
															document.getElementById("get_cool-hp_pro").disabled = onoff;
															document.getElementById("get_cool-hp_in").disabled = onoff;
															document.getElementById("get_cool-hp_dif").disabled = onoff;
															document.getElementById("get_cool-temp_pid").disabled = onoff;
															document.getElementById("get_cool-w").disabled = onoff;
															document.getElementById("get_cool-kw").disabled = onoff;
															document.getElementById("get_cool-hp_time").disabled = onoff;
														} else if(idsel == "get_heat-rule") {
															var onoff = k == 0;
															document.getElementById("get_heat-target").disabled = k == 2;
															//document.getElementById("get_heat-dtemp").disabled = k == 1;
															document.getElementById("get_heat-hp_pro").disabled = onoff;
															document.getElementById("get_heat-hp_in").disabled = onoff;
															document.getElementById("get_heat-hp_dif").disabled = onoff;
															document.getElementById("get_heat-temp_pid").disabled = onoff;
															document.getElementById("get_heat-w").disabled = onoff;
															document.getElementById("get_heat-kw").disabled = onoff;
															document.getElementById("get_heat-hp_time").disabled = onoff;
														} else if(idsel == "get_cool-target") {
															document.getElementById("get_cool-temp2").disabled = k == 0;
															document.getElementById("get_cool-temp1").disabled = k != 0;
														} else if(idsel == "get_heat-target") {
															document.getElementById("get_heat-temp2").disabled = k == 0;
															document.getElementById("get_heat-temp1").disabled = k != 0;
														}
													} else selected = false;
													if(idsel == "get_listchart") {
														var elems = document.getElementsByName("chrt_sel");
														for(var j = 0; j < elems.length; j++) {
															elems[j].add(new Option(cont2[0],cont2[0], false, selected), null); // "_"+
														}
													} else {
														var opt = new Option(cont2[0], k, false, selected);
														if(cont2[2] == 0) opt.disabled = true;
														element.add(opt, null);
													}
												}
											} else {
												for(var k = 0; k < cont1.length - 1; k++) {
													var cont2 = cont1[k].split(':');
													if(cont2[1] == 1) { element.innerHTML = cont2[0]; break; } 
												}
											}
										}
									}
								} else if(type == 'const') {
									var element = document.getElementById(valueid);
									if(element && values[0] != null && values[0] != 0 && values[1] != null && values[1] != 0) {
										if(values[0] == 'get_status') {
											element.innerHTML = "<div>" + values[1].replace(/>/g, "'>").replace(/\|/g, "</div><div id='") + "</div>";
										} else {
											if(values[0] == "CONST") values[1] = "VER_WEB|Версия веб-страниц|" + VER_WEB + ';' + values[1];
											element.innerHTML = "<tr><td>" + values[1].replace(/\|/g, "</td><td>").replace(/(\;)/g, "</td></tr><tr><td>") + "</td></tr>";
										}
									}
								} else if(type == 'table') {
									if(values[1] != null && values[1] != 0) {
										if(values[0] == 'get_tblInput') {
											var content = "", content2 = "", upsens = "", loadsens = "";
											var count = values[1].split(';');
											for(var j = 0; j < count.length - 1; j++) {
												input = count[j].toLowerCase();
												loadsens = loadsens + "get_alarmInput(" + count[j] + "),get_eInput(" + count[j] + "),get_typeInput(" + count[j] + "),get_pinInput(" + count[j] + "),get_Input(" + count[j] + "),get_nInput(" + count[j] + "),get_testInput(" + count[j] + "),";
												upsens = upsens + "get_Input(" + count[j] + "),get_eInput(" + count[j] + "),";
												content = content + '<tr><td>' + count[j] + '</td><td id="get_ninput-' + input + '"></td><td id="get_input-' + input + '">-</td> <td nowrap><input id="get_alarminput-' + input + '" type="number" min="0" max="1"><input type="submit" value=">" onclick="setParam(\'get_alarmInput(' + count[j] + ')\');"></td><td nowrap><input id="get_testinput-' + input + '" type="number" min="0" max="1" step="1" value=""><input type="submit" value=">"  onclick="setParam(\'get_testInput(' + count[j] + ')\');"></td><td id="get_pininput-' + input + '">-</td><td id="get_typeinput-' + input + '">-</td><td id="get_einput-' + input + '">-</td>';
												content = content + '</tr>';
											}
											document.getElementById(valueid).innerHTML = content;
											updateParam(upsens);
											loadParam(loadsens);
										} else if(values[0] == 'get_tblTempF') {
											var content = "", upsens = "", loadsens = "";
											var tnum = 1;
											element = document.getElementById(valueid);
											if(!element) {
												element = document.getElementById(valueid + '2');
												if(element) tnum = 2; // set
											}
											var count = values[1].split(';');
											for(var j = 0; j < count.length - 1; j++) {
												var T = count[j];
												loadsens += "get_eTemp(" +T+ "),";
												upsens += "get_eTemp(" +T+ "),";
												if(tnum == 1) {
													loadsens += "get_maxTemp(" +T+ "),get_errTemp(" +T+ "),get_esTemp(" +T+ "),get_minTemp(" +T+ "),get_fTemp4(" +T+ "),get_fTemp5(" +T+ "),get_fTemp6(" +T+ "),get_nTemp(" +T+ "),get_testTemp(" +T+ "),get_bTemp(" +T+ "),";
													upsens += "get_fullTemp(" +T+ "),get_esTemp(" +T+ "),";
												} else if(tnum == 2) {
													loadsens += "get_aTemp(" +T+ "),get_fTemp1(" +T+ "),get_fTemp2(" +T+ "),get_fTemp3(" +T+ "),get_nTemp2(" +T+ "),get_bTemp(" +T+ "),";
													upsens += "get_rawTemp(" +T+ "),";
												}
												T = T.toLowerCase();
												content += '<tr>';
												content += '<td>' +count[j]+ '</td>';
												if(tnum == 1) {
													content += '<td id="get_ntemp-' +T+ '"></td>';
													content += '<td id="get_fulltemp-' +T+ '">-</td>';
													content += '<td id="get_mintemp-' +T+ '">-</td>';
													content += '<td id="get_maxtemp-' +T+ '">-</td>';
													content += '<td nowrap><input id="get_errtemp-' +T+ '" type="number" step="0.01"><input type="submit" value=">" onclick="setParam(\'get_errTemp(' + count[j] + ')\');"></td>';
													content += '<td nowrap><input id="get_testtemp-' +T+ '" type="number" step="0.1"><input type="submit" value=">" onclick="setParam(\'get_testTemp(' + count[j] + ')\');"></td>';
													content += '<td nowrap><input type="checkbox" id="get_ftemp4-' +T+ '" onchange="setParam(\'get_fTemp4(' +count[j]+')\');"><input type="checkbox" id="get_ftemp5-' +T+ '" onchange="setParam(\'get_fTemp5(' +count[j]+')\');"><input type="checkbox" id="get_ftemp6-' +T+ '" onchange="setParam(\'get_fTemp6(' +count[j]+')\');"></td>';
													content += '<td id="get_btemp-' +T+ '">-</td>';
													content += '<td id="get_estemp-' +T+ '">-</td>';
												} else if(tnum == 2) {
													content += '<td id="get_ntemp2-' +T+ '"></td>';
													content += '<td id="get_rawtemp-' +T+ '">-</td>';
													content += '<td nowrap><span id="get_btemp-' +T+ '">-</span>:<span id="get_atemp-' +T+ '">-</span></td>';
													content += '<td><select id="set_atemp-' +T+ '" onchange="setParam(\'set_aTemp(' +count[j]+ ')\');"></select></td>';
													content += '<td nowrap><input type="checkbox" id="get_ftemp1-' +T+ '" onchange="setParam(\'get_fTemp1(' +count[j]+')\');"><input type="checkbox" id="get_ftemp2-' +T+ '" onchange="setParam(\'get_fTemp2(' +count[j]+')\');"><input type="checkbox" id="get_ftemp3-' +T+ '" onchange="setParam(\'get_fTemp3(' +count[j]+ ')\');"></td>';
												}
												content += '<td id="get_etemp-' +T+ '">-</td>';
												content += '</tr>';
												if(loadsens.length >= 1200) {
													loadParam(loadsens);
													loadsens = "";
												}
											}
											element.innerHTML = content;
											if(loadsens.length) loadParam(loadsens);
											updateParam(upsens);
										} else if(values[0].substr(0, 11) == 'get_tblTemp') {
											var content = "", loadsens = "", upsens = "";
											var count = values[1].split(';');
											for(var j = 0; j < count.length - 1; j++) {
												var T = count[j];
												loadsens += "get_nTemp(" +T+ "),";
												upsens += "get_fullTemp(" +T+ "),";
												T = T.toLowerCase();
												content += '<tr><td nowrap><span id="get_ntemp-' +T+ '"></span>:</td><td id="get_fulltemp-' +T+ '"></td></tr>';
											}
											document.getElementById(valueid).innerHTML = content;
											loadParam(loadsens);
											updateParam(upsens);
										} else if(values[0] == 'get_tblFlow') {
											var content = "", upsens = "", loadsens = "";
											var count = values[1].split(';');
											for(var j = 0; j < count.length - 1; j++) {
												input = count[j].toLowerCase();
												loadsens = loadsens + "get_nFlow(" + count[j] + "),get_Flow(" + count[j] + "),get_checkFlow(" + count[j] + "),get_minFlow(" + count[j] + "),get_kfFlow(" + count[j] + "),get_cFlow(" + count[j] + "),get_frFlow(" + count[j] + "),get_testFlow(" + count[j] + "),get_pinFlow(" + count[j] + "),get_eFlow(" + count[j] + "),";
												upsens = upsens + "get_Flow(" + count[j] + "),get_frFlow(" + count[j] + "),get_eFlow(" + count[j] + "),";
												content = content + '<tr>';
												content = content + '<td>' + count[j] + '</td>';
												content = content + '<td id="get_nflow-' + input + '">-</td>';
												content = content + '<td nowrap><span id="get_flow-' + input + '">-</span> <input id="ClcFlow' + input + '" type="submit" value="*" onclick="CalcAvgValue(\''+ input + '\')"></td>';
												content = content + '<td nowrap><input id="get_checkflow-' + input + '" type="checkbox" onChange="setParam(\'get_checkFlow(' + count[j] + ')\');"><input id="get_minflow-' + input + '" type="number" min="0.1" max="25.5" step="0.1"><input type="submit" value=">" onclick="setParam(\'get_minFlow(' + count[j] + ')\');"></td>';
												content = content + '<td nowrap><input id="get_kfflow-' + input + '" type="number" min="0.01" max="655" step="0.01" style="max-width:70px;" value=""><input type="submit" value=">"  onclick="setParam(\'get_kfFlow(' + count[j] + ')\');"></td>';
												content = content + '<td nowrap><input id="get_cflow-' + input + '" type="number" min="0" max="65535" step="1" value=""><input type="submit" value=">"  onclick="setParam(\'get_cFlow(' + count[j] + ')\');"></td>';
												content = content + '<td id="get_frflow-' + input + '">-</td>';
												content = content + '<td nowrap><input id="get_testflow-' + input + '" type="number" min="0.0" max="1000" step="0.001" value=""><input type="submit" value=">"  onclick="setParam(\'get_testFlow(' + count[j] + ')\');"></td>';
												content = content + '<td id="get_pinflow-' + input + '">-</td>';
												content = content + '<td id="get_eflow-' + input + '">-</td>';
												content = content + '</tr>';
											}
											document.getElementById(valueid).innerHTML = content;
											updateParam(upsens);
											loadParam(loadsens);
										} else if(values[0] == 'get_tblRelay') {
											var content = "", upsens = "", loadsens = "";
											var count = values[1].split(';');
											for(var j = 0; j < count.length - 1; j++) {
												if((relay = count[j].toLowerCase()) == "") continue;
												loadsens = loadsens + "get_pinRelay(" + count[j] + "),get_isRelay(" + count[j] + "),get_nRelay(" + count[j] + "),";
												upsens = upsens + "get_Relay(" + count[j] + "),";
												content = content + '<tr id="get_isrelay-' + relay + '"><td>' + count[j] + '</td><td id="get_nrelay-' + relay + '"></td><td id="get_pinrelay-' + relay + '"></td><td><span id="get_relay-' + relay + '-ONOFF"></span><input type="checkbox" name="relay" id="get_relay-' + relay + '" onchange="setParam(\'get_Relay(' + count[j] + ')\');"></td>';
												content = content + '</tr>';
											}
											document.getElementById(valueid).innerHTML = content;
											updateParam(upsens);
											loadParam(loadsens);
										} else if(values[0] == 'get_tblPwrC') {
											var content = "";
											var count = values[1].split(';');
											for(var j = 0; j < count.length - 1; j++) {
												content = content + '<tr><td>' + count[j] + '</td><td nowrap><input id="get_pwrc-' + count[j].toLowerCase() + '" type="number" value="' + count[j+1] + '"><input type="submit" value=">" onclick="setParam(\'get_PwrC(' + count[j++] + ')\');"></td></tr>';
											}
											document.getElementById(valueid).innerHTML = content;
										} else if(values[0] == 'get_numberIP') {
											var content = "", content2 = "", upsens = "", loadsens = "";
											count = Number(values[1]);
											for(var j = 1; j < count + 1; j++) {
												upsens = upsens + "get_sensorIP(" + j + "),";
												loadsens = loadsens + "get_slIP(" + j + "),get_sensorRuleIP(" + j + "),get_sensorUseIP(" + j + "),";
												content = content + '<tr id="get_sensorip-' + j + '"></tr>';
												content2 = content2 + '<tr><td><input type="checkbox" id="get_sensoruseip-' + j + '" onchange="setParam(\'get_sensorUseIP(' + j + ')\');" ></td>';
												content2 = content2 + '<td><input type="checkbox" id="get_sensorruleip-' + j + '" onchange="setParam(\'get_sensorRuleIP(' + j + ')\');" ></td>';
												content2 = content2 + '<td><select id="get_slip-' + j + '" onchange="setParam(\'get_slIP-' + j + '\',\'get_slip-' + j + '\');"></select></td></tr>';
											}
											document.getElementById(valueid).innerHTML = content;
											document.getElementById(valueid + "-inputs").innerHTML = content2;
											updateParam(upsens);
											loadParam(loadsens);
										} else if(values[0] == 'get_Prof(NUM)') {
											var content = "", loadsens = "";
											count = Number(values[1]);
											for(var j = 0; j < count; j++) {
												loadsens = loadsens + "infoProfile(" + j + "),";
												content = content + '<tr id="get_prof-' + j + '"><td>' + (j+1) + '</td><td id="infoprofile-' + j + '"></td>';
												content = content + '<td nowrap><input id="eraseprofile-' + j + '" type="submit" value="Стереть"  onclick=\'loadParam("eraseProfile(' + j + ')")\'> <input name="profile" type="submit" value="Загрузить" onclick=\'loadParam("loadProfile(' + j + ')")\' disabled></td></tr>';
											}
											document.getElementById(valueid).innerHTML = content;
											loadParam(loadsens);
										} else if(values[0] == 'get_tblPDS') {
											var content = "";
											var trows = values[1].split('|');
											var elem = document.getElementById("get_listdsr");
											for(var j = 0; j < trows.length - 1; j++) {
												var tflds = trows[j].split(';');
												content += '<tr><td><select id="get_prof-dsd' + j + '" onchange="setParam(\'get_Prof(DSD' + j + ')\')">' + elem.innerHTML.replace('>' + tflds[0] + '<', ' selected>' + tflds[0] + '<') + '<\select></td><td>' + tflds[1] 
													+ '</td><td nowrap><input id="get_prof-dss' + j + '" type="text" size="6" value="' + tflds[2] + '"> <input type="submit" value=">" onclick="setDS(\'S\',' + j + ')"></td><td nowrap><input id="get_prof-dse' + j + '" type="text" size="6" value="' + tflds[3] + '"> <input type="submit" value=">" onclick="setDS(\'E\',' + j + ')"></td></tr>';
											}
											document.getElementById(valueid).innerHTML = content;
										} else {
											var content = values[1].replace(/</g, "&lt;").replace(/\|$/g, "").replace(/\|/g, "</td><td>").replace(/\n/g, "</td></tr><tr><td>");
											var element = document.getElementById(valueid);
											if(element) element.innerHTML = '<td>' + content + '</td>';
										}
									} else {
										element = document.getElementById(values[0]);
										if(element) element.innerHTML = "";
									}
								} else if(type == 'values') {
									if(valueid == "get_ohp-time_chart") window.time_chart = values[1].toLowerCase().replace(/[^\w\d]/g, "");
									element3 = document.getElementById(valueid + "3");
									if(element3) {
										element3.value = values[1];
										element3.innerHTML = values[1];
									}
									element = document.getElementById(valueid);
									if(element) {
										if(element.className == "charsw") {
											element.innerHTML = element.title.substr(values[1].toLowerCase().replace(/[^\w\d]/g, ""), 1);
										} else if(/^E\d+/.test(values[1])) {
											if(element.getAttribute("type") == "submit") alert("Ошибка " + values[1]);
											else element.placeholder = values[1];
										} else if(element != document.activeElement) {
											element.innerHTML = values[1];
											element.value = element.type == "number" ? values[1].replace(/[^-0-9.,]/g, "") : values[1];
										}
									}
								} else if(type == 'is') {
									if(values[1] == 0 || values[1].substring(0,1) == 'E') {
										if((element = document.getElementById(valueid))) element.className = "inactive";
										if(values[0].indexOf("RHEAT")!=-1) {
											if((element = document.getElementById("get_ohp-add_heat"))) element.disabled = true;
										}
										if(values[0].indexOf("REVI")!=-1) {
											if((element = document.getElementById("get_ohp-temp_evi"))) element.disabled = true;
											if((element = document.getElementById("get_ohp-temp_evi2"))) element.disabled = true;
										}
										if((element = document.getElementById(valueid))) {
											element = element.getElementsByTagName('select');
											if(element[0]) element[0].disabled = true;
										}
									}
								} else if(type == 'tbv') {
									var element2 = document.getElementById(valueid.replace("val", "err"));
									if(values[1].match(/^E-?\d/)) {
										if(element2) element2.innerHTML = values[1]; 
									} else {
										if(element2) element2.innerHTML = "OK";
										if((element = document.getElementById(valueid))) {
											element.value = values[1];
											element2 = document.getElementById(valueid.replace("val", "hex"));
											if(element2) element2.value = "0x" + Number(values[1]).toString(16).toUpperCase();
										}
									}
								} else if(values[0] == "get_WORK") {
									element = document.getElementById(values[0].toLowerCase());
									var onoff = values[1] == "ON"; // "OFF"
									if(element) element.innerHTML = onoff ? "ВКЛ" : "ВЫКЛ";   
									element = document.getElementById("onoffswitch");
									if(element) element.checked = onoff;
									if((element=document.getElementById('set_zeroEEV'))) element.disabled = onoff;
									if((element=document.getElementById('get_testmode'))) element.disabled = onoff;
									//if((element=document.getElementById('get_listprof'))) element.disabled = onoff;
									//if((element=document.getElementById('get_modehp'))) element.disabled = onoff;
									if((element=document.getElementById('scan'))) element.disabled = onoff;
									element = document.getElementById('manual_override'); if(element && element.checked) onoff = false;
									var elements = document.getElementsByName('profile');
									for(var j = 0; j < elements.length; j++) elements[j].disabled = onoff;
									var elements = document.getElementsByName('relay');
									for(var j = 0; j < elements.length; j++) elements[j].disabled = onoff;
									if((element=document.getElementById('get_peev-pos'))) element.disabled = onoff;
									if((element=document.getElementById('get_peev-posp'))) element.disabled = onoff;
									if((element=document.getElementById('set-eev'))) element.disabled = onoff;
									if((element=document.getElementById('set-eevp'))) element.disabled = onoff;
									if((element=document.getElementById('get_pfc-on_off'))) element.disabled = onoff;
								} else if(values[0] == "get_uptime") {
									if((element = document.getElementById("get_uptime"))) element.innerHTML = values[1];
									if((element = document.getElementById("get_uptime2"))) element.innerHTML = values[1];
								} else if(values[0] == "get_errcode" && values[1] == 0) {
									document.getElementById("get_errcode").innerHTML = "OK";
									document.getElementById("get_error").innerHTML = "";
								} else if(values[0] == "get_errcode" && values[1] < 0) {
									document.getElementById("get_errcode").innerHTML = "Ошибка";
								} else if(values[0] == "test_Mail") {
									setTimeout(loadParam('get_Message(scan_MAIL)'), 3000);
								} else if(values[0] == "test_SMS") {
									setTimeout(loadParam('get_Message(scan_SMS)'), 3000);
								} else if(values[0] == "progFC") {
									alert(values[1]);
								} else if(values[0].indexOf("set_SAVE")==0) {
									if(values[1] >= 0) {
										if(values[0].match(/SCHDLR$/)) alert("Настройки расписаний сохранены!");
										else if(values[0].match(/STATS$/)) alert("Статистика сохранена!");
										else if(values[0].match(/UPD$/)) alert("Успешно!");
										else alert("Настройки сохранены, записано " + values[1] + " байт");
									} else alert("Ошибка, код: " + values[1]);
								} else if(values[0] == "RESET_DUE" || values[0] == "RESET_JOURNAL" || values[0] == "set_updateNet" || values[0] == "RESET_ErrorFC") {
									alert(values[1]);
								} else if(values[0].toLowerCase() == "set_off" || values[0].toLowerCase() == "set_on") {
									break;
								} else {
									if((element = document.getElementById(valueid))) {
										element.value = values[1];
										element.innerHTML = values[1];
									}
									if((element = document.getElementById(valueid + "2"))) {
										element.value = values[1];
										element.innerHTML = values[1];
									}
								}
							}
						}
						if(typeof window["loadParam_after"] == 'function') window["loadParam_after"](paramid);
					} // end: if (request.responseText != null)
				} // end: if (request.responseText != null)
			} else if(request.status == 0) return;
			else if(noretry != true) {
				if(request.status == 401 && location.protocol == "file:") NeedLogin = 1;
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

function dhcp(dcb) {
	var fl = document.getElementById(dcb).checked;
	document.getElementById('get_net-ip').disabled = fl;
	document.getElementById('get_net-subnet').disabled = fl;
	document.getElementById('get_net-gateway').disabled = fl;
	document.getElementById('get_net-dns').disabled = fl;
	document.getElementById('get_net-ip2').disabled = fl;
	document.getElementById('get_net-subnet2').disabled = fl;
	document.getElementById('get_net-gateway2').disabled = fl;
	document.getElementById('get_net-dns2').disabled = fl;
}

function validip(valip) {
	var valid = /\b(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\b/.test(document.getElementById(valip).value);
	if(!valid) alert('Сетевые настройки введены неверно!');
	return valid;
}

function validmac(valimac) {
	var valid = /^[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}$/.test(document.getElementById(valimac).value);
	if(!valid) alert('Аппаратный mac адрес введен неверно!');
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

var upload_error = false;

function upload(file) {
	var xhr = new XMLHttpRequest();
//	xhr.upload.onprogress = function(event) { console.log(event.loaded + ' / ' + event.total); }
//	xhr.onload = xhr.onerror = function() {
//		if(this.status == 200) { console.log("success"); } else { console.log("error " + this.status); }
//	};
	xhr.open("POST", urlcontrol + (NeedLogin == 2 ? "/&&" + LPString : ""), false);
	xhr.setRequestHeader('Title', file.settings ? "*SETTINGS*" : encodeURIComponent(file.name));
	xhr.onreadystatechange = function() {
		if(this.readyState != 4) return;
		if(xhr.status == 200) {
			if(xhr.responseText != null && xhr.responseText != "") {
				upload_error = true;
				alert(xhr.responseText);
			}
		}
	}
	xhr.send(file);
}

function autoheight() {
/*	var max_col_height = Math.max(
		document.body.scrollHeight, document.documentElement.scrollHeight,
		document.body.offsetHeight, document.documentElement.offsetHeight,
		document.body.clientHeight, document.documentElement.clientHeight
	);*/
	var max_col_height = window.innerHeight;
	var columns = document.body.children;
	for(var i = columns.length - 1; i >= 0; i--) { // прокручиваем каждую колонку в цикле
		if(columns[i].offsetHeight > max_col_height) {
			max_col_height = columns[i].offsetHeight; // устанавливаем новое значение максимальной высоты
		}
	}
	for(var i = columns.length - 1; i >= 0; i--) {
		columns[i].style.minHeight = max_col_height; // устанавливаем высоту каждой колонки равной максимальной
	}
	document.body.style.minWidth = Math.max(document.body.clientWidth, document.body.scrollWidth);
}

function calcacp() {
	var a1 = document.getElementById("a1").value;
	var a2 = document.getElementById("a2").value;
	var p1 = document.getElementById("p1").value;
	var p2 = document.getElementById("p2").value;
	kk2 = (p2 - p1) * 100 / (a2 - a1);
	kk1 = p1 * 100 - kk2 * a1;
	document.getElementById("k1").innerHTML = Math.abs(Math.round(kk1));
	document.getElementById("k2").innerHTML = Math.abs(kk2.toFixed(3));
}

function setKanalog() {
	var k1 = document.getElementById("k1").innerHTML;
	var k2 = document.getElementById("k2").innerHTML;
	var sens = document.getElementById("get_listpress").selectedOptions[0].text;
	if(sens == "--") return;
	var elem = document.getElementById("get_transpress-" + sens.toLowerCase());
	if(elem) {
		elem.value = k2;
		elem.innerHTML = k2;
	}
	elem = document.getElementById("get_zeropress-" + sens.toLowerCase());
	if(elem) {
		elem.value = k1;
		elem.innerHTML = k1;
	}
	setParam('get_zeroPress(' + sens + ')');
	setParam('get_transPress(' + sens + ')');
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
	document.cookie = name + '="' + escape(value) + '";expires=' + (new Date(2100, 1, 1)).toUTCString();
}