# Copyright 2018 plutoo
from RemoteConnection import RemoteConnection
import usb.core
import usb.util
from usb.core import USBError
import struct

def get_string_safe(dev, index, default="Unknown"):
    """Безопасно получить строковый дескриптор: None/Unknown вместо исключений."""
    if not index:
        return None
    # Кандидаты: сначала явный langid устройства (если есть), потом фолбэк 0x0409
    candidates = []
    try:
        langids = getattr(dev, "langids", ())
    except Exception:
        langids = ()
    if langids:
        candidates.append(langids[0])
    if 0x0409 not in candidates:
        candidates.append(0x0409)

    for lid in candidates:
        try:
            s = usb.util.get_string(dev, index, langid=lid)
            if s:
                return s
        except (USBError, ValueError, Exception):
            continue
    return default

class RemoteConnectionUsb(RemoteConnection):
    def _is_bulk_or_intr(self, ep):
        t = ep.bmAttributes & 0x03  # 0=ctrl,1=iso,2=bulk,3=intr
        return t in (2, 3)

    def _setup_eps(self):
        # 1) Попробуем отстегнуть драйвер ядра (Linux)
        try:
            cfg = self.dev.get_active_configuration()
            for intf in cfg:
                if hasattr(self.dev, "is_kernel_driver_active") and \
                self.dev.is_kernel_driver_active(intf.bInterfaceNumber):
                    try:
                        self.dev.detach_kernel_driver(intf.bInterfaceNumber)
                        print(f"Detach kernel driver: iface {intf.bInterfaceNumber}")
                    except USBError as e:
                        print(f"Не удалось detach kernel driver: {e}")
        except Exception:
            pass

        self.cfg = self.dev.get_active_configuration()
        self.intf = None
        self.ep_in = None
        self.ep_out = None

        # 2) Перебираем все интерфейсы и альтернативные настройки,
        #    ищем первую пару IN/OUT с типом bulk/interrupt
        for intf in self.cfg:
            # Явно активируем altsetting, если что
            try:
                self.dev.set_interface_altsetting(
                    interface=intf.bInterfaceNumber,
                    alternate_setting=intf.bAlternateSetting
                )
            except Exception:
                pass

            cand_in = None
            cand_out = None
            for ep in intf:
                if not self._is_bulk_or_intr(ep):
                    continue
                if usb.util.endpoint_direction(ep.bEndpointAddress) == usb.util.ENDPOINT_IN and cand_in is None:
                    cand_in = ep
                if usb.util.endpoint_direction(ep.bEndpointAddress) == usb.util.ENDPOINT_OUT and cand_out is None:
                    cand_out = ep

            if cand_in and cand_out:
                self.intf = intf
                self.ep_in = cand_in
                self.ep_out = cand_out
                break

        # 3) Если не нашли — пробуем старый фолбэк (0,0)
        if self.intf is None:
            try:
                intf = self.cfg[(0, 0)]
                cand_in = usb.util.find_descriptor(
                    intf,
                    custom_match=lambda e: _is_bulk_or_intr(e) and
                        usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN
                )
                cand_out = usb.util.find_descriptor(
                    intf,
                    custom_match=lambda e: _is_bulk_or_intr(e) and
                        usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT
                )
                if cand_in and cand_out:
                    self.intf = intf
                    self.ep_in = cand_in
                    self.ep_out = cand_out
            except Exception:
                pass

        # 4) Финальная проверка с понятным сообщением
        if self.ep_in is None or self.ep_out is None:
            # Выкинем все, что знаем — для диагностики
            print("\nДиагностика интерфейсов/эндпоинтов:")
            for intf in self.cfg:
                print(f"  Iface {intf.bInterfaceNumber}/{intf.bAlternateSetting}")
                for ep in intf:
                    typ = ep.bmAttributes & 0x03
                    typ_s = {0:'CTRL',1:'ISO',2:'BULK',3:'INTR'}.get(typ, '?')
                    dire = 'IN' if usb.util.endpoint_direction(ep.bEndpointAddress)==usb.util.ENDPOINT_IN else 'OUT'
                    print(f"    EP 0x{ep.bEndpointAddress:02x} {dire} {typ_s} wMax {ep.wMaxPacketSize}")
            raise RuntimeError(
                "Не удалось найти пару IN/OUT bulk/interrupt эндпоинтов. "
                "Если видите только Control EP0 — ваше устройство не поддерживает нужный режим обмена."
            )

        print(f"\nИспользуется интерфейс: {self.intf.bInterfaceNumber}/{self.intf.bAlternateSetting}")
        print(f"IN  endpoint: 0x{self.ep_in.bEndpointAddress:02x} (wMax={self.ep_in.wMaxPacketSize})")
        print(f"OUT endpoint: 0x{self.ep_out.bEndpointAddress:02x} (wMax={self.ep_out.wMaxPacketSize})")

    def __init__(self):
        super().__init__()
        self.debug = True
        self._last_app_pid = None
 
    def readResponse(self):
         result, size = struct.unpack('<II', self.read(0x8))
         return {'rc': result, 'size': size}
 
    def checkResult(self, resp):
         if resp['rc'] != 0:
             raise SwitchError('SwitchError 0x%x' % resp['rc'])
 
    # --- добавим хелпер, который запускает прослушку и возвращает PID ---
    def cmdListenForAppLaunch(self):
        """
        Отправляет команду "слушать запуск приложения" и возвращает PID запущенного приложения.
        Возврат: int pid
        """
        
        resp = self.readResponse()
        self.checkResult(resp)
        payload = b""
        if resp['size'] > 0:
            payload = self.read(resp['size'])
        # предполагаем, что payload минимум 4 байта с PID
        if len(payload) < 4:
            raise SwitchError("Неверный формат ответа listen-for-app-launch (ожидался PID)")
        (pid,) = struct.unpack('<I', payload[:4])
        self._last_app_pid = pid
        if self.debug:
            print(f"[listen] app pid = {pid}")
        return pid

    def waitForAppLaunchPid(self, timeout=None):
        """
        Совместимость с вашим вызовом из MainWindow: просто возвращает
        последний PID, который установил cmdListenForAppLaunch().
        """
        if self._last_app_pid is None:
            raise RuntimeError("PID ещё не получен: сначала вызовите cmdListenForAppLaunch()")
        return self._last_app_pid


    def __init__(self):
        super().__init__()
        self.debug = True

        # Скан устройств
        devices = list(usb.core.find(find_all=True))
        print("\nНайденные USB устройства:")
        for dev in devices:
            try:
                print(f"Vendor ID: 0x{dev.idVendor:04x}, Product ID: 0x{dev.idProduct:04x}")
                mfg = get_string_safe(dev, getattr(dev, "iManufacturer", 0)) or "Unknown"
                prod = get_string_safe(dev, getattr(dev, "iProduct", 0)) or "Unknown"
                print(f"Name: {mfg} - {prod}")
                # bus/addr/port/speed доступны не на всех платформах — берём мягко
                bus = getattr(dev, "bus", "?")
                addr = getattr(dev, "address", "?")
                port = getattr(dev, "port_number", "?")
                speed = getattr(dev, "speed", "?")
                print(f"Bus: {bus}, Address: {addr}")
                print(f"Port: {port}, Speed: {speed}")
                if dev.idVendor == 0x057e and dev.idProduct == 0x3000:
                    print("*** NINTENDO SWITCH DETECTED ***")
                print("---")
            except Exception as e:
                print(f"Не удалось получить информацию об устройстве: {e}")

        # Ищем нужное устройство (Switch 057e:3000 -> если нет, пробуем 18d1:4ee0)
        self.dev = usb.core.find(idVendor=0x057e, idProduct=0x3000) \
                   or usb.core.find(idVendor=0x18d1, idProduct=0x4ee0)
        if self.dev is None:
            raise Exception('Устройство не найдено')

        try:
            print("\nИнформация об устройстве:")
            print(f"Manufacturer: {get_string_safe(self.dev, getattr(self.dev, 'iManufacturer', 0))}")
            print(f"Product: {get_string_safe(self.dev, getattr(self.dev, 'iProduct', 0))}")

            # Сбросить (не обязательно — делаем мягко)
            try:
                self.dev.reset()
                print("Устройство сброшено")
            except Exception:
                print("Не удалось сбросить устройство (не критично)")

            # Активировать конфигурацию
            try:
                self.cfg = self.dev.get_active_configuration()
            except USBError:
                print("Пробуем установить конфигурацию...")
                self.dev.set_configuration()
                self.cfg = self.dev.get_active_configuration()
            self._setup_eps()
            print(f"Активная конфигурация: {self.cfg.bConfigurationValue}")

            # Попробуем аккуратно отсоединить драйвер ядра на Linux
            try:
                for intf in self.cfg:
                    if self.dev.is_kernel_driver_active(intf.bInterfaceNumber):
                        try:
                            self.dev.detach_kernel_driver(intf.bInterfaceNumber)
                            print(f"Detach kernel driver: интерфейс {intf.bInterfaceNumber}")
                        except USBError as e:
                            print(f"Не удалось detach kernel driver: {e}")
                usb.util.dispose_resources(self.dev)  # на всякий случай
            except Exception:
                pass

            # Выберем интерфейс и эндпоинты по признакам
            print("\nДоступные интерфейсы и эндпоинты:")
            chosen_intf = None
            ep_in = ep_out = None

            for intf in self.cfg:
                print(f"Интерфейс {intf.bInterfaceNumber}:")
                for ep in intf:
                    direction = usb.util.endpoint_direction(ep.bEndpointAddress)
                    dir_str = "IN" if direction == usb.util.ENDPOINT_IN else "OUT"
                    print(f"  Endpoint 0x{ep.bEndpointAddress:02x}: {dir_str}, "
                          f"type: {ep.bmAttributes & 0x03}, "
                          f"max pkt: {ep.wMaxPacketSize}")
                    # Первые подходящие bulk/interrupt берем как кандидаты
                    if direction == usb.util.ENDPOINT_IN and ep_in is None:
                        ep_in = ep
                        chosen_intf = intf
                    if direction == usb.util.ENDPOINT_OUT and ep_out is None:
                        ep_out = ep
                        chosen_intf = intf

            if chosen_intf is None or ep_in is None or ep_out is None:
                # Фолбэк к (0,0), если вдруг не нашли
                chosen_intf = self.cfg[(0, 0)]
                ep_in = usb.util.find_descriptor(
                    chosen_intf,
                    custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress)
                                           == usb.util.ENDPOINT_IN
                )
                ep_out = usb.util.find_descriptor(
                    chosen_intf,
                    custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress)
                                           == usb.util.ENDPOINT_OUT
                )

            if ep_in is None:
                raise Exception("Не найден IN endpoint")
            if ep_out is None:
                raise Exception("Не найден OUT endpoint")

            self.intf = chosen_intf
            self.ep_in = ep_in
            self.ep_out = ep_out

            print(f"\nИспользуется интерфейс: {self.intf.bInterfaceNumber}")
            print(f"IN endpoint:  0x{self.ep_in.bEndpointAddress:02x}")
            print(f"OUT endpoint: 0x{self.ep_out.bEndpointAddress:02x}")

        except usb.core.USBError as e:
            print(f"Ошибка при инициализации USB: {str(e)}")



    def _clear_halt_safe(self, ep):
        try:
            self.dev.clear_halt(ep)
            return True
        except Exception:
            return False

    def write(self, data):
        if self.debug:
            print(f"\nОтправляем пакет размером {len(data)} байт:")
            print(f"Hex: {' '.join(f'{b:02x}' for b in data)}")
            print(f"ASCII: {' '.join((chr(b) if 32 <= b <= 126 else '.') for b in data)}")

        if not hasattr(self, "ep_out") or self.ep_out is None:
            raise RuntimeError("USB не инициализирован (нет OUT endpoint)")

        size = len(data)
        total_written = 0
        timeout = 5000
        retry_left = 20

        while size > 0:
            try:
                max_packet_size = self.ep_out.wMaxPacketSize
                chunk = data[:max_packet_size]
                n = self.ep_out.write(chunk, timeout=timeout)
                if n <= 0:
                    raise USBError("[write] zero bytes written")
                total_written += n
                data = data[n:]
                size -= n
            except USBError as e:
                msg = str(e)
                if "timed out" in msg or getattr(e, 'errno', None) == 110:
                    if self.debug: print("write: timeout, retry")
                    retry_left -= 1; 
                    if retry_left <= 0: break
                    time.sleep(0.05)
                    continue
                if "Broken pipe" in msg or getattr(e, 'errno', None) in (32,):
                    if self.debug: print("write: EPIPE, clear_halt & retry")
                    self._clear_halt_safe(self.ep_out)
                    retry_left -= 1
                    if retry_left <= 0: break
                    continue
                if "Input/Output Error" in msg or getattr(e, 'errno', None) in (5,):
                    if self.debug: print("write: EIO, clear_halt & retry once")
                    self._clear_halt_safe(self.ep_out)
                    retry_left -= 1
                    if retry_left <= 0: break
                    continue
                if "No such device" in msg or getattr(e, 'errno', None) in (19,):
                    raise RuntimeError("USB устройство отключилось / сменило режим (ENODEV)") from e
                raise

        if size != 0:
            raise Exception(f"Не удалось записать данные полностью. Отправлено {total_written} байт")
        return total_written

    def read(self, size):
        if self.debug:
            print(f"\nЧитаем {size} байт...")

        if not hasattr(self, "ep_in") or self.ep_in is None:
            raise RuntimeError("USB не инициализирован (нет IN endpoint)")

        data = bytearray()
        timeout = 5000
        idle_retry = 20  # сколько раз терпим таймауты, собирая ответ

        while len(data) < size and idle_retry > 0:
            try:
                max_packet = self.ep_in.wMaxPacketSize
                need = size - len(data)
                chunk = self.ep_in.read(min(need, max_packet), timeout=timeout)
                if not chunk:
                    if self.debug: print("read: пустой чанк")
                    idle_retry -= 1
                    continue
                if self.debug:
                    print(f"Получено {len(chunk)} байт:")
                    print(f"Hex: {' '.join(f'{b:02x}' for b in chunk)}")
                data.extend(chunk)
            except USBError as e:
                msg = str(e)
                if "timed out" in msg or getattr(e, 'errno', None) == 110:
                    if self.debug: print("read: timeout, waiting more…")
                    idle_retry -= 1
                    continue
                if "Broken pipe" in msg or getattr(e, 'errno', None) in (32,):
                    if self.debug: print("read: EPIPE, clear_halt & retry")
                    self._clear_halt_safe(self.ep_in)
                    idle_retry -= 1
                    continue
                if "Input/Output Error" in msg or getattr(e, 'errno', None) in (5,):
                    if self.debug: print("read: EIO, clear_halt & retry")
                    self._clear_halt_safe(self.ep_in)
                    idle_retry -= 1
                    continue
                if "No such device" in msg or getattr(e, 'errno', None) in (19,):
                    raise RuntimeError("USB устройство отключилось / сменило режим (ENODEV)") from e
                raise

        if len(data) < size:
            raise Exception(f"Не удалось прочитать данные. Прочитано {len(data)} из {size} байт")
        return bytes(data)
