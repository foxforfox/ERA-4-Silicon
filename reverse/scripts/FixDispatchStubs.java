// Repairs ERA 4 binaries where Intel IPP CPU-dispatch stubs
//   lea rax,[level]; movsxd rax,[rax]; lea r11,[table]; jmp [r11+rax*8]
// were wrongly marked "no return" (Ghidra follows the table's first entry into the
// IPP init stub), which cuts off all code after each call. Clears no-return on every
// stub and on the init routine, re-disassembles after their call sites, repairs the
// calling functions, renames stubs ipp_<addr>, then decompiles an address range.
//   -postScript FixDispatchStubs.java <out.c> <startHex> <endHex>
// Run on the saved project (no -readOnly) so the repair is kept for later runs.
//@category ERA
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.PrintWriter;
import java.util.*;

public class FixDispatchStubs extends GhidraScript {
    boolean isDispatchStub(Function f) {
        Instruction i = getInstructionAt(f.getEntryPoint());
        for (int n = 0; n < 3 && i != null; n++) i = i.getNext();
        return i != null && i.getMnemonicString().equals("JMP")
            && i.toString().contains("R11 + RAX*0x8");
    }

    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        Address start = toAddr(a[1]), end = toAddr(a[2]);
        List<Function> fixed = new ArrayList<>();
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            boolean stub = isDispatchStub(f);
            if (stub || f.getEntryPoint().getOffset() == 0x2860
                || (f.isThunk() && f.getThunkedFunction(true).getEntryPoint().getOffset() == 0x2860)) {
                f.setNoReturn(false);
                fixed.add(f);   // repair call sites even if the flag was only inherited
            }
            if (stub && f.getName().startsWith("FUN_"))
                f.setName("ipp_" + f.getEntryPoint().toString(), SourceType.ANALYSIS);
        }
        Set<Function> callers = new HashSet<>();
        for (Function f : fixed) {
            for (Reference r : getReferencesTo(f.getEntryPoint())) {
                Instruction call = getInstructionAt(r.getFromAddress());
                if (call == null || !call.getFlowType().isCall()) continue;
                call.setFlowOverride(FlowOverride.NONE);
                call.clearFallThroughOverride();
                Address next = call.getMaxAddress().next();
                if (getInstructionAt(next) == null)
                    new DisassembleCommand(next, null, true).applyTo(currentProgram, monitor);
                Function c = getFunctionContaining(call.getAddress());
                if (c != null) callers.add(c);
            }
        }
        for (Function c : callers)
            CreateFunctionCmd.fixupFunctionBody(currentProgram, c, monitor);
        println("cleared no-return on " + fixed.size() + ", repaired " + callers.size() + " callers");

        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        try (PrintWriter out = new PrintWriter(a[0])) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(new AddressSet(start, end), true)) {
                DecompileResults r = d.decompileFunction(f, 120, monitor);
                out.println("// ==== " + f.getEntryPoint() + "  " + f.getName() + "\n");
                out.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// decompile failed\n");
            }
        }
    }
}
