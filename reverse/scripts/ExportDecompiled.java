// Ghidra headless post-script: writes every function's decompiled C, with its address,
// to <output dir>/<program>.c, plus the vtables Ghidra recovered from RTTI to .vtables.txt.
// Run via reverse/scripts/decompile.sh. Output is for studying the algorithms only.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;

public class ExportDecompiled extends GhidraScript {
    @Override
    public void run() throws Exception {
        String outDir = getScriptArgs().length > 0 ? getScriptArgs()[0] : ".";
        String name = currentProgram.getName();
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        try (PrintWriter out = new PrintWriter(new FileWriter(new File(outDir, name + ".c")))) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (monitor.isCancelled()) break;
                if (f.isThunk() || f.isExternal()) continue;
                DecompileResults r = decomp.decompileFunction(f, 60, monitor);
                out.println("// ==== " + f.getEntryPoint() + "  " + f.getName(true));
                out.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// decompile failed: " + r.getErrorMessage());
            }
        }
        try (PrintWriter out = new PrintWriter(new FileWriter(new File(outDir, name + ".vtables.txt")))) {
            SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
            while (it.hasNext()) {
                Symbol s = it.next();
                String n = s.getName(true);
                if (n.contains("vtable") || n.contains("typeinfo") || n.startsWith("vftable"))
                    out.println(s.getAddress() + "  " + n);
            }
        }
        decomp.dispose();
    }
}
