# Normal manual gameplay. The launcher supplies no direct-level argument.
set pagination off
set confirm off
set print thread-events off
handle SIGSEGV stop print nopass
python
import gdb
import struct

class GameFailure(gdb.Breakpoint):
    def stop(self):
        gdb.write('GAME_FAILURE %s\n' % self.location)
        gdb.execute('bt 32')
        gdb.execute('info args')
        if self.location == 'game_memory_fail':
            width = int(gdb.parse_and_eval('sizeof(void*)'))
            stack = int(gdb.parse_and_eval('$rsp' if width == 8 else '$esp'))
            try:
                words = struct.unpack('<' + ('Q' if width == 8 else 'I') * 96,
                    bytes(gdb.selected_inferior().read_memory(stack, width * 96)))
                for i, address in enumerate(words):
                    symbol = gdb.execute('info symbol 0x%x' % address, to_string=True)
                    if not symbol.startswith('No symbol'):
                        gdb.write('STACK_SYMBOL[%u] %x %s' % (i, address, symbol))
            except gdb.error as error:
                gdb.write('Raw stack unavailable: %s\n' % error)
        return True

for function in ('game_memory_fail', 'gl1_fail', 'application_failure'):
    try:
        GameFailure(function)
    except gdb.error as error:
        gdb.write('WARNING: failure breakpoint unavailable: %s\n' % error)
end
run
python
code = gdb.parse_and_eval('$_exitcode')
if code.type.code == gdb.TYPE_CODE_VOID:
    gdb.write('GAME_STOPPED_BEFORE_EXIT\n')
    gdb.execute('bt 32')
    gdb.execute('info registers')
    gdb.execute('info sharedlibrary')
    gdb.execute('quit 1')
else:
    gdb.write('GAME_PROCESS_EXIT code=%d\n' % int(code))
    gdb.execute('quit %d' % int(code))
end
