// Debug helper: prints how Ghidra sees one call site and its target.
//   -postScript ProbeCall.java <callAddrHex>
//@category ERA
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;

public class ProbeCall extends GhidraScript {
    @Override
    public void run() throws Exception {
        Address at = toAddr(getScriptArgs()[0]);
        Instruction i = getInstructionAt(at);
        println("instr " + i + " flow=" + i.getFlowType() + " override=" + i.getFlowOverride()
                + " fallthrough=" + i.getFallThrough() + " overridden=" + i.isFallThroughOverridden());
        Address next = i.getMaxAddress().next();
        println("next instr " + getInstructionAt(next) + " in fn " + getFunctionContaining(next));
        for (Address t : i.getFlows()) {
            Function f = getFunctionAt(t);
            println("target " + t + " fn=" + f + " noReturn=" + (f != null && f.hasNoReturn())
                    + " thunk=" + (f != null && f.isThunk()));
            if (f != null && f.isThunk()) {
                Function g = f.getThunkedFunction(true);
                println("  thunked " + g + " noReturn=" + g.hasNoReturn());
            }
        }
        Function c = getFunctionContaining(at);
        println("caller " + c + " body=" + c.getBody().getMinAddress() + ".." + c.getBody().getMaxAddress());
    }
}
