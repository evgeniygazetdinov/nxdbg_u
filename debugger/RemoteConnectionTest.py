# Copyright 2018 plutoo
from RemoteConnection import *

DUMMY_ADDR_SPACE = [
    (       0,         0x100000, 0,  0), # Unmapped
    (0x100000,           0x2000, 5,  3), # CodeStatic
    (0x102000, (1<<48)-0x102000, 0,  0), # Unmapped
    (   1<<48,            1<<64, 0, 16),
]

class RemoteConnectionTest(RemoteConnection):
    def __init__(self):
        RemoteConnection.__init__(self)
        self.dummy_data = b"Test data"
        self.event_counter = 0

    def read(self, size):
        # Возвращаем тестовые данные
        return self.dummy_data[:size]

    def write(self, data):
        # В тестовом режиме просто принимаем данные
        return len(data)

    def cmdDetachProcess(self, handle): # Cmd2
        pass

    def cmdQueryMemory(self, handle, addr): # Cmd3
        for r in DUMMY_ADDR_SPACE:
            if addr >= r[0] and addr < (r[0] + r[1]):
                return {'addr': r[0], 'size': r[1], 'perm': r[2], 'type': r[3]}
        raise Exception("Address not found in dummy space")

    def cmdGetDbgEvent(self, handle): # Cmd4
        # В тестовом режиме возвращаем None, что означает отсутствие событий
        return None

    def cmdReadMemory(self, handle, addr, size): # Cmd5
        # Возвращаем тестовые данные для чтения памяти
        return b"0" * size

    def cmdContinueDbgEvent(self, handle, flags, thread_id): # Cmd6
        pass

    def cmdGetThreadContext(self, handle, thread_id, flags): # Cmd7
        # Возвращаем тестовый контекст потока
        return {
            'pc': 0x100000,
            'sp': 0x200000,
            'registers': [i for i in range(31)]
        }

    def cmdBreakProcess(self, handle): # Cmd8
        pass

    def cmdWriteMemory32(self, handle, addr, val): # Cmd9
        pass
