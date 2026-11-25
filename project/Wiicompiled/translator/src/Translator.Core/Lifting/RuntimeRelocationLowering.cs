using System.Globalization;
using Translator.Core.Disassembly;
using Translator.Core.Ir;

namespace Translator.Core.Lifting;

// RSO operands are relocated by the guest loader. Read them before SSA so the
// optimizer cannot bake the analysis image's temporary section addresses in.
public static class RuntimeRelocationLowering
{
    private static readonly HashSet<uint> Sites = LoadSites();
    private static HashSet<uint> LoadSites()
    {
        var path = Environment.GetEnvironmentVariable("RECOMP_RUNTIME_IMMEDIATES");
        return string.IsNullOrEmpty(path) ? new() : File.ReadLines(path)
            .Where(s => !string.IsNullOrWhiteSpace(s))
            .Select(s => uint.Parse(s.Trim(), NumberStyles.HexNumber, CultureInfo.InvariantCulture)).ToHashSet();
    }
    public static IReadOnlyList<IrInstruction> Rewrite(PpcInstruction instruction, IReadOnlyList<IrInstruction> original)
    {
        if (!Sites.Contains(instruction.Address)) return original;
        uint op = instruction.Word >> 26;
        if (op is not (13 or 14 or 15 or 32 or 33 or 34 or 36 or 37 or 38 or 40 or 42 or 44 or 48 or 50 or 52))
            throw new InvalidOperationException($"Unsupported RSO immediate at {instruction.Address:X8}: opcode {op}");
        long expected = op == 15 ? (long)(short)(instruction.Word & 65535) << 16 : (short)(instruction.Word & 65535);
        string temp = $"addr_relocimm_{instruction.Address:X8}_loc", ea = $"addr_relocea_{instruction.Address:X8}_loc";
        var value = IrValue.Register(temp);
        var result = new List<IrInstruction> { new IrCall(temp, "PPC_ReadRelocatedImmediate", new[] { IrValue.Imm(instruction.Address), IrValue.Imm(op == 15 ? 1 : 0) }) };
        bool changed = false;
        bool Match(IrValue v) => v.Kind == "const" && v.Constant == expected;
        bool AddressMatch(IrAddress a) => op >= 32 && (op & 1) == 0 && a.Base == $"r{(instruction.Word >> 16) & 31}" && a.Offset == expected;
        foreach (var node in original)
        {
            switch (node)
            {
                case IrAssign a when Match(a.Value): result.Add(a with { Value = value }); changed = true; break;
                case IrBinary b when Match(b.Right): result.Add(b with { Right = value }); changed = true; break;
                case IrLoad l when AddressMatch(l.Address):
                    result.Add(new IrBinary(ea, IrValue.Register(l.Address.Base), value, "add"));
                    result.Add(l with { Address = new IrAddress(ea, 0) }); changed = true; break;
                case IrStore s when AddressMatch(s.Address):
                    result.Add(new IrBinary(ea, IrValue.Register(s.Address.Base), value, "add"));
                    result.Add(s with { Address = new IrAddress(ea, 0) }); changed = true; break;
                case IrCall c when c.Target == "PPC_UpdateCarryAdd" && c.Arguments.Count > 1 && Match(c.Arguments[1]):
                    var args = c.Arguments.ToArray(); args[1] = value;
                    result.Add(c with { Arguments = args }); changed = true; break;
                default: result.Add(node); break;
            }
        }
        if (!changed) throw new InvalidOperationException($"RSO immediate was not lowered at {instruction.Address:X8} ({instruction.Mnemonic})");
        return result;
    }
}
