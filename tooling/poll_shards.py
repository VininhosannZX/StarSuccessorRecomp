"""Build-only interrupt safe points. Canonical Wiicompiled output stays untouched.
Snapshot all resident architectural variables before servicing a back-edge.
The host interrupt bridge preserves the interrupted CPU's register image.
"""
import re
VERSION=3

def instrument(source):
 parts=re.split(r'(?=^extern "C")',source,flags=re.M);count=0
 for idx,part in enumerate(parts):
  if 'CpuContext* MKW_RESTRICT ctx)' not in part:continue
  first=part.find('    goto loc_')
  if first<0:continue
  variables=re.findall(r'^    (?:uint32_t|PPC_FPR) (\w+) = (ctx->[^;]+);$',part[:first],re.M)
  if not variables:continue
  positions={m.group(1):m.start() for m in re.finditer(r'^(loc_[0-9A-F]+):',part,re.M)}
  def patch(m):
   nonlocal count
   name=m.group(2)
   if name not in positions or positions[name]>=m.start():return m.group(0)
   count+=1;indent=m.group(1)
   snapshot='\n'.join(indent+'    '+field+' = '+name+';' for name,field in variables)
   return indent+'if (StarPollPending(ctx)) {\n'+snapshot+'\n'+indent+'    StarPollInvoke(ctx);\n'+indent+'}\n'+m.group(0)
  parts[idx]=re.sub(r'^(\s*)goto (loc_[0-9A-F]+);',patch,part,flags=re.M)
 source=''.join(parts)
 source=re.sub(r'switch \(static_cast<uint32_t>\(([^\n]+)\)\) \{', r'switch (StarRsoVirtualTarget(static_cast<uint32_t>(\1))) {', source)
 return '#include "star_poll.h"\n'+source,count
