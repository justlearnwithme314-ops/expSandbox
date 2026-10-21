"""Generate the initial editable content pack; no external Python packages needed."""
import json
from pathlib import Path

items = []
recipes = []

def item(id, name, color, **kwargs):
    items.append(dict(id=id, name=name, color=color, **kwargs))

def recipe(id, name, inputs, output, station="hand"):
    recipes.append(dict(id=id, name=name, inputs=inputs, output=output, station=station))

blocks = [
    ("core:air", "Air", [0,0,0], dict(solid=False, placeable=False)),
    ("core:grass", "Meadow grass", [105,151,73], {}),
    ("core:dirt", "Earth", [137,96,64], {}),
    ("core:stone", "Stone", [141,146,151], dict(hardness=1.8)),
    ("core:log", "Oak log", [126,91,57], dict(hardness=1.3)),
    ("core:leaves", "Oak leaves", [75,130,61], dict(hardness=.2)),
    ("core:sand", "Sand", [216,196,139], dict(hardness=.5)),
    ("core:gravel", "Gravel", [139,133,128], dict(hardness=.6)),
    ("core:snow", "Snow", [225,235,239], dict(hardness=.5)),
    ("core:coal_ore", "Coal ore", [76,79,81], dict(drop="survival:coal",hardness=2)),
    ("core:iron_ore", "Iron ore", [174,137,115], dict(hardness=2.5)),
    ("core:diamond_ore", "Diamond ore", [79,204,203], dict(drop="frontier:diamond",hardness=4)),
    ("core:bedrock", "Bedrock", [57,60,66], dict(hardness=1000)),
    ("building:planks", "Oak planks", [186,143,86], {}),
    ("building:bricks", "Red brick", [174,89,67], {}),
    ("building:glass", "Glass", [146,201,212], {}),
    ("building:torch", "Torch", [242,183,67], dict(solid=False)),
    ("building:bed", "Bed", [191,73,67], {}),
    ("building:tnt", "TNT", [216,76,60], {}),
    ("adventure:gate", "Sanctum gate", [166,101,208], {}),
    ("survival:chest", "Storage chest", [163,113,54], {}),
    ("crafting:workbench", "Workbench", [143,100,57], {}),
    ("crafting:furnace", "Furnace", [92,97,103], {}),
    ("industry:generator", "Coal generator", [183,140,59], {}),
    ("industry:battery", "Energy battery", [71,149,157], {}),
    ("industry:crusher", "Ore crusher", [140,155,174], {}),
    ("survival:crop", "Wheat crop", [176,185,74], dict(solid=False,drop="survival:wheat_seeds",hardness=.1)),
]
for id, name, color, options in blocks:
    item(id, name, color, kind="block", **options)

for id,name,color,food in [
    ("survival:apple","Apple",[207,70,57],5),
    ("survival:meat","Raw meat",[207,130,113],3),
    ("survival:bread","Bread",[200,156,78],7),
    ("survival:wheat","Wheat",[212,190,88],0),
    ("survival:wheat_seeds","Wheat seeds",[122,171,72],0),
    ("survival:coal","Coal",[54,57,62],0),
    ("survival:stick","Stick",[156,117,72],0),
    ("survival:arrow","Arrow",[201,192,167],0),
    ("frontier:iron_ingot","Iron ingot",[196,206,209],0),
    ("frontier:diamond","Diamond",[80,220,219],0),
    ("industry:iron_dust","Iron dust",[166,127,108],0),
]:
    item(id,name,color,kind="item",placeable=False,food=food)

for tier,color,speed,durability,damage in [
    ("wood",[175,127,74],1.7,100,3),
    ("stone",[151,156,160],2.5,250,4),
    ("iron",[203,214,219],3.7,600,6),
    ("diamond",[75,215,213],5.5,1400,8),
]:
    for tool in ["pickaxe","axe","shovel","sword","hoe"]:
        item(f"frontier:{tier}_{tool}",f"{tier.title()} {tool}",color,kind="tool",placeable=False,stack=1,durability=durability,speed=speed,damage=damage+(2 if tool=="sword" else 0))
item("survival:bow","Bow",[192,151,85],kind="tool",placeable=False,stack=1,durability=300,damage=6)

recipe("crafting:planks","Saw planks",[["core:log",1]],["building:planks",4])
recipe("crafting:sticks","Carve sticks",[["building:planks",2]],["survival:stick",4])
recipe("crafting:workbench","Build workbench",[["building:planks",4]],["crafting:workbench",1])
recipe("crafting:chest","Build chest",[["building:planks",8]],["survival:chest",1])
recipe("crafting:furnace","Build furnace",[["core:stone",8]],["crafting:furnace",1])
recipe("crafting:torch","Make torches",[["survival:coal",1],["survival:stick",1]],["building:torch",4])
recipe("crafting:bread","Bake bread",[["survival:wheat",3]],["survival:bread",1])
recipe("crafting:arrows","Make arrows",[["core:stone",1],["survival:stick",2]],["survival:arrow",8])
recipe("crafting:bow","Build bow",[["survival:stick",3],["core:leaves",3]],["survival:bow",1])
recipe("crafting:bricks","Masonry",[["core:stone",4]],["building:bricks",4])
recipe("crafting:bed","Build bed",[["building:planks",3],["core:leaves",3]],["building:bed",1])
recipe("crafting:tnt","Build TNT",[["core:sand",4],["survival:coal",4]],["building:tnt",1],"crafting:workbench")
for tier,material in [("wood","building:planks"),("stone","core:stone"),("iron","frontier:iron_ingot"),("diamond","frontier:diamond")]:
    for tool,cost in [("pickaxe",3),("axe",3),("shovel",1),("sword",2),("hoe",2)]:
        recipe(f"crafting:{tier}_{tool}",f"{tier.title()} {tool}",[[material,cost],["survival:stick",2]],[f"frontier:{tier}_{tool}",1],"hand" if tier=="wood" else "crafting:workbench")
for machine in ["generator","battery","crusher"]:
    recipe(f"crafting:{machine}",f"Build {machine}",[["frontier:iron_ingot",4],["core:stone",4]],[f"industry:{machine}",1],"crafting:workbench")

root=Path(__file__).resolve().parents[1]
(root/"content").mkdir(exist_ok=True)
(root/"content/base.json").write_text(json.dumps(dict(items=items,recipes=recipes),indent=2)+"\n",encoding="utf-8")
print(f"Wrote {len(items)} items and {len(recipes)} recipes")
