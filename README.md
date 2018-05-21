# ControlHeatPump
Контроллер теплового насоса

Открытый проект "Народный контроллер" для тепловых насосов.<br>
Данное програмноое обеспечение предназначено для управления<br>
различными типами тепловых насосов для отопления и ГВС.<br>
<br>
Подробное обсуждение проекта на форуме: <br>
https://www.forumhouse.ru/threads/352693/ <br>
https://www.forumhouse.ru/threads/437182/

Изменения по версиям: <br>
Версия 0.952
1. Добавлено в управление ЭРВ возможность корректировки перегерва в зависмости от разницы температур испарителя и конденсатора
2. Оптимизация сохранения ЭРВ
3. Фикс: DHСP, датчики температуры. Оптимизация веба.
4. Опция - один датчик на 2-ой шине 1-Wire, для уменьшения влияния помех (измененен формат сохранения: 123)
5. DNS по TCP (выбор автоматически), доработка сетевой либы, оптимизация журнала и др.
6. Начало переделки парсера запросов с целью его оптимизации и увеличения скорости.
7.

Версия 0.951
1. Доработка веб, перенос настроек в config, исправление багов
2. Доработка страницы тестирования
3. Добавлен расчет переохлаждение на схеме ТН

Версия 0.950
1. Доработка 1-Wire, багфиксы, новая ошибка ERR_ONEWIRE_RW - ошибка во время чтения/записи OneWire
2. Фикс библиотеки DUE для доступности портов D53,D69
3. Доработка I2C init
4. Оптимизация Web-modbus

Версия 0.949
1. Оптимизация использования стека в itoa, vsnprintf, ftoa cj,собсвенная функция printf
2. Patch Arduino board SAM library for optimized itoa() function
3. Добавлен насос нагрева ГВС RPUMPBH (не циркуляции ГВС)
4. Фикс отображения включения ТН во время паузы
5. Добавлена кнопка сброса статистики задач free RTOS
6. Исправлена ошибка перехода в режим Ожидания по расписанию
7. Для экономии памяти теперь графики расчета разности температур ПТО рассчитываются в момент запроса
8. Добавлен насос теплых полов RPUMPFL (в режиме охлаждения не включается)
9. Перевод FLOWPCON в не обязательный параметр
10. Автоскрытие аналогового управления инвертером, насоса ТП
11. Доработка по инвертору Vacon (режим тест)

Версия 0.948
1. Уменьшение потребление стека RTOS
2. Фикс ожидания DHCP, оптимизация сетевой либы

Версия 0.947
1. При инициализации сетевого чипа добавлена проверка на соединение сетевым кабелем (link)
2. Переделан код обслуживания ТЭНа бойлера, теперь он сосредоточен в одном месте
3. Оптимизация и переименование кода для переключения между ГВС и отоплением/охлаждением (поддержка управления насосами для переключения)
4. Исправлена ошибка с сохранением/восстановлением настроек в бинарный файл
5. Доделана и протестирована страница Modbus (можно читать/сохранять регистры)
6. Сделан алгоритм обеззараживания (сальмонелла) для ГВС
7. Исправлена ошибка в расписании ГВС

Версия 0.946
1. Исправлена ошибка отображения графика потока конденсатора
2. Исправлена ошибка вывода в текстовый файл настроек (коэффициент для аналогового датчика)
3. Исправлена ошибка управленем догревом бойлера - алгоритм перенесен в другое место
4. Исправлена ошибка в режиме тест для датчиков потока
5. Настройки догрева бойлера перенесены в раздел ГВС из опций (откорректированы имена параметров запросов)
6. Сделан вывод специфических ошибок для Vacon 10 и пофиксены некоторые ошибки
7. Добавлена возможность проверки потоков на минимальные значения перед включением компрессора (#define FLOW_CONTROL)
8. Доработана морда для сканирования после изменения работы шины OneWire, в новой версии увеличилось время сканирования.
9. Дмитрий обновил мобильную морду
10. Добавлен функционал модификации имен запрашиваемых файлов, теперь в зависимости от конфига показываются на морде разные схемы.
11. В работу ТН добавлены внутренние команды WAIT и RESUME для обеспечения функционала расписания, теперь если в расписании дырка (нет профайла) то ТН будет находится в сосоянии "Ожидание"
12. ТЕСТ страничка с чтением/записью произвольных ячеек Modbus RTU

Версия 0.945
1. R3WAY переведен в необязательный, теперь можно компилировать без него
2. При переключениеи с ГВС сделано временное отключение (#define DELAY_BOILER_OFF) части защит, на время остывания ТН.
3. Немного переделана функция вывода ошибок, теперь где актуально выводится имя функции в которой произошла ошибка
4. В уведомлениях почты убран вывод устройство которых нет в текущей конфигурации (если стоит флаг выводить подробности)
5. Исправлен и добавлен вывод настроек ТН в текстовом виде (дабавлены настроки тарифов)
6. Проведено переименование констант, переменных и функция инвертора  в нейтральные (без упоменния ОМРОН и МХ2)
7. Добавлена поддержка трехфазного счетчика SDM630 (vad711) (определить #define USE_SDM630)
8. Класс частотный преобразователь был стандартизован (все аппартано зависимые функции спрятаны внутрь класса) для возможности подключения других инверторв короме Omrona
9. vad711 добавил поддержку инвертора Vacon 10 по модбасу.
10. Исправлена ошибка подсчета общего числа ошибок чтения датчиков температуры
11. Переделана блокировка задачи циркуляционного насоса отопления в паузах
12. Исправлена ошибка вывода информации при старте ТН
13. Переделан механизм работы с датчиками температуры. Теперь разрешено три вида шин OneWire и одновременно может присутвовать три вида шин в контроллере.
14. Добавлено расписание работы (пока не до конца)
15. Переделан вид расписания бойлера на веб морде
16. Добавлена страница на вебморду для вывода информации по задачам операционной системы (главное - минимальный размер стека)
17. Часть нагрузки из задачи "бездействия" перенесено в задачу чтения датчиков
18. На морде в профилях сделано обновление данных при изменении параметров профиля
19. В связи с работой над расписаниями, удален функционал тарифов день/ночь
20. Исправена ошибка вывода числа ошибок чтения температурных датчиков.
21. Добавлен ввод теплоемкости теплоносителя в контурах где стоят расходомеры (изменилась версия сохранения)
22. Переделана размерность коэффициентов пересчета расходомеров, теперь она в имп/литр
