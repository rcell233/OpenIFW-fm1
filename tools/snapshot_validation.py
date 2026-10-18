#!/usr/bin/env python3
"""Retain current numeric evidence with the original factory programs."""
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[1]
out=root/'docs/evidence';out.mkdir(exist_ok=True)
files=['test-results.json','fixed-results.json','fixed-engine-results.json','fixed-factory-results.json','factory-results.json','firmware-environment.json','test-phase2.log','sanitizer-phase2.log','fixed-sanitizer.log','integration.log','panel-final.log','firmware-final.log']
for name in files:
 p=root/'build'/name
 if p.is_file():shutil.copy2(p,out/name)
for name,source in [('firmware-wasm-results.json','build/emu/results.json'),('factory-regression-results.json','build/factory-regression/results.json'),('core-v3.log','build/core-v3.log'),('web-validation.log','build/web-validation.log'),('factory-regression.log','build/factory-regression.log')]:
 p=root/source
 if p.is_file():shutil.copy2(p,out/name)
p=root/'build/firmware/build.json'
if p.is_file():shutil.copy2(p,out/'firmware-build.json')
print('Retained numeric reports; curated TEST_RESULTS.md is unchanged.')
