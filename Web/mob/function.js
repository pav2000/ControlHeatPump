var xmlHttp=createXmlHttpObject();
function createXmlHttpObject(){
 if(window.XMLHttpRequest){
  xmlHttp=new XMLHttpRequest();
 }else{
  xmlHttp=new ActiveXObject('Microsoft.XMLHTTP');
 }
 return xmlHttp;
}

function load(){
  load1();
  load2();
  load3(); 
} 
 function load1() { if(xmlHttp.readyState==0 || xmlHttp.readyState==4){
  xmlHttp.open('PUT','http://62.140.252.108:2545/&get_Temp(TOUT)&&',true);
  xmlHttp.send(null);
  xmlHttp.onload = function(e) {
   jsonResponse=JSON.parse(xmlHttp.responseText);
   loadBlock(); }
 }
 }
function load2(){
  if(xmlHttp.readyState==0 || xmlHttp.readyState==4){
  xmlHttp.open('PUT','http://62.140.252.108:2545/&get_minTemp(TOUT)&&',true);
  xmlHttp.send(null);
  xmlHttp.onload = function(m) {
   jsonResponse=JSON.parse(xmlHttp.responseText);
   loadBlock2();}
 }
}

function load3(){
  if(xmlHttp.readyState==0 || xmlHttp.readyState==4){
  xmlHttp.open('PUT','http://62.140.252.108:2545/&get_Temp(TIN)&&',true);
  xmlHttp.send(null);
  xmlHttp.onload = function(m) {
   jsonResponse=JSON.parse(xmlHttp.responseText);
   loadBlock3();}
 }
}


function loadBlock(data2) {
 data2 = JSON.parse(xmlHttp.responseText);
 data = document.getElementsByTagName('body')[0].innerHTML;
 var new_string;
for (var key in data2) {
 new_string = data.replace(new RegExp('{{'+key+'}}', 'g'), data2[key]);
 data = new_string;
}
 document.getElementsByTagName('body')[0].innerHTML = new_string;
}


function loadBlock2(data3) {
 data3 = JSON.parse(xmlHttp.responseText);
 data = document.getElementsByClassName('col-sm-10')[0].innerHTML;
 var new_string;
for (var key in data3) {
 new_string = data.replace(new RegExp('{{'+key+'}}', 'g'), data3[key]);
 data = new_string;
}
 document.getElementsByClassName('col-sm-10')[0].innerHTML = new_string;
}

function loadBlock3(data4) {
 data4 = JSON.parse(xmlHttp.responseText);
 data = document.getElementsByClassName('col-sm-10')[0].innerHTML;
 var new_string;
for (var key in data4) {
 new_string = data.replace(new RegExp('{{'+key+'}}', 'g'), data4[key]);
 data = new_string;
}
 document.getElementsByClassName('col-sm-10')[0].innerHTML = new_string;
}

function val(id){
 var v = document.getElementById(id).value;
 return v;
}
function send_request(submit,server){
 request = new XMLHttpRequest();
 request.open("GET", server, true);
 request.send();
 save_status(submit,request);
}
function save_status(submit,request){
 old_submit = submit.value;
 request.onreadystatechange = function() {
  if (request.readyState != 4) return;
  submit.value = request.responseText;
  setTimeout(function(){
   submit.value=old_submit;
   submit_disabled(false);
  }, 1000);
 }
 submit.value = 'Подождите...';
 submit_disabled(true);
}
function submit_disabled(request){
 var inputs = document.getElementsByTagName("input");
 for (var i = 0; i < inputs.length; i++) {
  if (inputs[i].type === 'submit') {inputs[i].disabled = request;}
 }
}
function toggle(target) {
 var curVal = document.getElementById(target).className;
 document.getElementById(target).className = (curVal === 'hidden') ? 'show' : 'hidden';
}
