using System.Globalization;
using System.Text.Json;
namespace Translator.Core.Translation;

/// <summary>Explicit, address-scoped limits for independently verified large functions.</summary>
internal static class FunctionBudgetOverrides
{
    private sealed record Limits(int MaxInstructions, int MaxBytes);
    private static readonly IReadOnlyDictionary<uint, Limits> Entries = Load();
    private static IReadOnlyDictionary<uint, Limits> Load()
    {
        var path = Environment.GetEnvironmentVariable("RECOMP_FUNCTION_BUDGETS");
        var entries = new Dictionary<uint, Limits>();
        if (string.IsNullOrWhiteSpace(path)) return entries;
        using var doc = JsonDocument.Parse(File.ReadAllText(path));
        foreach (var entry in doc.RootElement.EnumerateObject())
        {
            var key = entry.Name.StartsWith("0x", StringComparison.OrdinalIgnoreCase) ? entry.Name[2..] : entry.Name;
            uint address = uint.Parse(key, NumberStyles.HexNumber, CultureInfo.InvariantCulture);
            int instructions = entry.Value.GetProperty("max_instructions").GetInt32();
            int bytes = entry.Value.GetProperty("max_bytes").GetInt32();
            if ((address & 3) != 0 || instructions <= 0 || instructions > 131072 || bytes <= 0 || bytes > 0x80000)
                throw new InvalidDataException($"Invalid function budget for {entry.Name}");
            entries.Add(address, new Limits(instructions, bytes));
        }
        return entries;
    }
    public static TranslationOptions Apply(uint entryPoint, TranslationOptions options) =>
        Entries.TryGetValue(entryPoint, out var limits)
            ? options with { MaxInstructions = Math.Max(options.MaxInstructions, limits.MaxInstructions), MaxBytes = Math.Max(options.MaxBytes, limits.MaxBytes) }
            : options;
}
