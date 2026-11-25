from pathlib import Path
p=Path('deliverables/StarSuccessorBoot9');b=(Path('combined-build/generated/data_sections_init_blobs/_data.bin')).read_bytes();code=b[0x80311E80-0x802F5260:0x80311E80-0x802F5260+32];h=2166136261
for v in code:h=((h^v)*16777619)&0xFFFFFFFF
(p/'dsp_boot_program.h').write_text('#pragma once\n#include <cstdint>\nnamespace StarBoot {\ninline constexpr uint32_t kInitProgramHash = 0x%08Xu;\ninline constexpr uint8_t kInitProgram[32] = {%s};\n}\n'%(h,','.join(str(v) for v in code)))
print('INIT boot payload FNV-1a32',hex(h),code.hex())
