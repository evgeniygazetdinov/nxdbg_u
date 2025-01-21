
# NXDBG - Nintendo Switch Debugger

NXDBG - это отладчик для Nintendo Switch, состоящий из двух основных компонентов:
- Сервис отладки (C) для Nintendo Switch
- GUI клиент (Python) для ПК

#### 3. Поддерживаемые команды
| Команда | Код | Описание |
|---------|-----|----------|
| REQ_LIST_PROCESSES | 0 | Получение списка процессов |
| REQ_ATTACH_PROCESS | 1 | Подключение к процессу |
| REQ_DETACH_PROCESS | 2 | Отключение от процесса |
| REQ_QUERYMEMORY | 3 | Запрос информации о памяти |
| REQ_GET_DBGEVENT | 4 | Получение отладочных событий |
| REQ_READMEMORY | 5 | Чтение памяти процесса |
| REQ_CONTINUE_DBGEVENT | 6 | Продолжение после события |
| REQ_GET_THREADCONTEXT | 7 | Получение контекста потока |
| REQ_BREAK_PROCESS | 8 | Остановка процесса |
| REQ_WRITEMEMORY32 | 9 | Запись 32-битного значения в память |
| REQ_LISTENAPPLAUNCH | 10 | Отслеживание запуска приложений |
| REQ_GETAPPPID | 11 | Получение PID приложения |
| REQ_START_PROCESS | 12 | Запуск процесса |
| REQ_GET_TITLE_PID | 13 | Получение PID по Title ID |

#### 4. Обработка отладочных событий
- При возникновении события (точка останова, исключение и т.д.)
- Сервис получает событие через `svcGetDebugEvent`
- Отправляет информацию о событии на PC
- PC может продолжить выполнение через `REQ_CONTINUE_DBGEVENT`

#### 5. Работа с памятью
- Чтение памяти: `svcReadDebugProcessMemory`
- Запись памяти: `svcWriteDebugProcessMemory`
- Запрос информации: `svcQueryDebugProcessMemory`
- Ограничение на размер чтения: 0x1000 байт за раз

#### 6. Управление потоками
- Получение контекста: `svcGetDebugThreadContext`
- Поддержка разных флагов для получения регистров
- Работа со списком потоков процесса: `svcGetThreadList`

#### 7. Безопасность и ограничения
- Проверка версии ядра для совместимости
- Обработка ошибок и корректное освобождение ресурсов
- Лимиты на размеры операций с памятью

#### 8. Отладочный вывод
- При компиляции с DEBUG=1 выводит подробную информацию
- Логирует все USB команды и ответы
- Показывает статус инициализации и ошибки


## Требования

### Для сервиса отладки (service)
- devkitPro с libnx
- Переменные окружения:
  - DEVKITPRO
  - DEVKITARM

### Для GUI клиента (debugger)
- Python 3.x
- PyQt5
- virtualenv (рекомендуется)

## Установка

1. Клонируйте репозиторий:
```bash
git clone https://github.com/yourusername/nxdbg.git
cd nxdbg
```

### 2.Соберите сервис отладки:
cd service
make

### 3.Настройте окружение Python и установите зависимости:
```
cd ../debugger
python -m venv env
source env/bin/activate
pip install PyQt5
```
### Использование
```
Загрузите service.nsp на Nintendo Switch
```

### Запустите GUI клиент с одним из следующих параметров:

1.Подключиться к процессу по PID:
- python MainWindow.py --pid <pid>
### Подключиться к процессу по Title ID:
 -python MainWindow.py --titleid <titleid>
2. Ждать запуска приложения:
-python MainWindow.py --nextlaunch
### Тестовый режим:
- python MainWindow.py --test
### Функции отладчика

- Подключение к запущенным процессам
Просмотр и модификация памяти

- Управление точками останова
Просмотр регистров и стека

- Пошаговое выполнение
Дизассемблирование ARM кода


============================================
```
Структура проекта
service/ - Сервис отладки на C
source/ - Исходный код сервиса
nxdbg.json - Конфигурация для создания KIP
debugger/ - GUI клиент на Python
MainWindow.py - Основной класс GUI
RemoteConnection*.py - Модули связи с сервисом
Adapter*.py - Адаптеры для отображения информации
mainwindow.ui - Qt дизайн интерфейса
```
================================================
### not solved issues 
  ### debugger:
    - [ ] handle usb connection for abstract switch
    - [ ] write to 10 to service
  ### service:
    - [ ] abtain info about process and send to gui
    - [ ] how run service as worker
================================================
### solved issues
  ### service:
  - [v] compiling service
  - [v] print debug info
  ### debugger:
  - [v] moved to qt5 and python3
  - [v] worked test mode

Copyright © 2017 plutoo
