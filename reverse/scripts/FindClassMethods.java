// Ghidra post-script: for each class name given (as in the binary's RTTI, e.g. PlosiveRemoverProcessor),
// finds its typeinfo and vtable(s) and prints every vtable slot with the target function address.
// Usage (after decompile.sh): analyzeHeadless <proj> <name> -process -noanalysis -postScript FindClassMethods.java <out.txt> <Class> ...
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class FindClassMethods extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            Memory mem = currentProgram.getMemory();
            for (int a = 1; a < args.length; a++) {
                String mangled = args[a].length() + args[a];
                byte[] needle = (mangled + "\0").getBytes("US-ASCII");
                Address name = mem.findBytes(currentProgram.getMinAddress(), needle, null, true, monitor);
                out.println("== " + args[a] + " name string at " + name);
                if (name == null) continue;
                for (Address ti : pointersTo(name)) {                    // typeinfo: {vtable ptr, name ptr, ...}
                    Address typeinfo = ti.subtract(8);
                    out.println("  typeinfo at " + typeinfo);
                    for (Address vt : pointersTo(typeinfo)) {             // vtable: {offset, typeinfo, slots...}
                        out.println("  vtable (typeinfo slot) at " + vt);
                        for (int i = 0; i < 80; i++) {
                            long target = mem.getLong(vt.add(8 + 8L * i));
                            Address t = toAddr(target);
                            Function f = getFunctionAt(t);
                            if (f == null && target != 0) break;
                            out.println(String.format("    slot %2d  %s  %s", i, t, f == null ? "-" : f.getName()));
                        }
                    }
                }
            }
        }
    }

    private List<Address> pointersTo(Address target) throws Exception {
        List<Address> found = new ArrayList<>();
        byte[] bytes = new byte[8];
        long v = target.getOffset();
        for (int i = 0; i < 8; i++) bytes[i] = (byte) (v >> (8 * i));
        Memory mem = currentProgram.getMemory();
        Address at = currentProgram.getMinAddress();
        while ((at = mem.findBytes(at, bytes, null, true, monitor)) != null) {
            found.add(at);
            at = at.add(1);
        }
        return found;
    }
}
