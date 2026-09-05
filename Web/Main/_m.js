// Общая логика мобильной версии. Народный контроллер ТН.
// Подключается после scripts.js (десктопного, без изменений) и до _menu.js.

// Запоминание выбора версии живёт только до закрытия вкладки.
function _setui(v){ try{ sessionStorage.setItem('ui', v); }catch(e){} }

// Переключатель десктопная / мобильная версия. Пиктограммы инлайновым SVG:
// внешний .svg сервер отдал бы как text/html и браузер его не отрисовал бы.
function _uisw(){
 return '<div class="uisw" title="Десктопная / мобильная версия">'
  + '<span onclick="_setui(\'d\');location.href=\'index.html\'">'
  + '<svg viewBox="0 0 24 24"><path d="M2 3h20v13H2zM11 16h2v3h-2zM8 19h8v2H8z"/></svg></span>'
  + '<span class="on">'
  + '<svg viewBox="0 0 24 24"><path d="M6 1h12v22H6zM9 20h6v1H9z"/></svg></span>'
  + '</div>';
}

// Шапка и шторка. Вызывается из _menu.js / _menu-user.js во время разбора страницы.
function _mdraw(items){
 var h = '<input type="checkbox" id="nav">'
  + '<header class="top">'
  + '<label class="burger" for="nav"><b></b></label>'
  + '<div class="ttl">' + document.title.split('.')[0] + '</div>'
  + '<div class="onoffswitch">'
  + '<input type="checkbox" class="onoffswitch-checkbox" id="onoffswitch"'
  + ' onchange="swich(\'onoffswitch\')">'
  + '<label class="onoffswitch-label" for="onoffswitch">'
  + '<span class="onoffswitch-inner"></span><span class="onoffswitch-switch"></span></label>'
  + '</div></header>'
  + '<label class="scrim" for="nav"></label>'
  + '<nav class="drawer">'
  + '<a href="_index.html" class="logo"></a>'
  + '<div class="menu-profiles">'
  + '<span id="get_mode" style="color: red">-</span><br>'
  + '<span id="SCH-PR"></span><br>'
  + '<select id="get_listprof" onchange="setParam(\'get_listProf\',\'get_listprof\');"></select>'
  + '</div><ul>';
 for(var i = 0; i < items.length; i++){
  var it = items[i];
  h += '<li class="' + it[0] + '"><a href="' + it[1] + '"'
    + (it[3] ? ' onclick="' + it[3] + '"' : '') + '><i></i>' + it[2] + '</a></li>';
 }
 h += '</ul><div class="dateinfo"><div id="get_status"></div></div>'
   + _uisw() + '</nav>';
 document.write(h);
 _mactive();
 loadParam("get_listProf");
 updateParam("get_status,get_WORK,get_MODE,get_SCHDLR(On),get_listProf_");
}

// Подсветка активного пункта - тот же приём, что в menu.js:79-85, но имя файла
// может начинаться с подчёркивания, а класс пишется без него: _heat.html -> "heat".
function _mactive(){
 var m = location.pathname.match(/_?[a-z0-9-]+\.html/);
 var cls = m ? m[0].replace(/^_/, '').replace(/(-set.*)?\.html$/, '') : document.body.className;
 if(!cls) return;
 var els = document.getElementsByClassName(cls);
 for(var i = 0; i < els.length; i++) els[i].classList.add('active');
}

// Журнал без jQuery: scripts.js:852 updatelog() тянет её ради одной строки.
function _updatelog(){
 var el = document.getElementById('log');
 if(!el) return;
 var x = new XMLHttpRequest();
 x.open('GET', urlcontrol + '/journal.txt', true);
 x.onreadystatechange = function(){
  if(x.readyState != 4) return;
  if(x.status == 200){
   el.textContent = x.responseText;
   el.scrollTop = el.scrollHeight;
  } else el.textContent = 'Ошибка загрузки журнала: ' + x.status;
 };
 x.send(null);
}

// Схема ТН свёрстана в 930x400 абсолютным позиционированием, поэтому вписываем
// её в ширину экрана масштабированием - подписи едут вместе с картинкой.
function _fitplan(){
 var w = document.getElementById('viswrap');
 var v = document.getElementById('vis');
 if(!w || !v) return;
 var k = w.clientWidth / 930;
 if(k > 1) k = 1;
 v.style.transform = 'scale(' + k + ')';
 w.style.height = Math.ceil(400 * k) + 'px';
}
