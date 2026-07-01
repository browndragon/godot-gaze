#!/usr/bin/env python3
import os
import glob
import xml.etree.ElementTree as ET

def get_text_recursive(node):
    if node is None:
        return ""
    text = node.text or ""
    for child in node:
        text += get_text_recursive(child)
        if child.tail:
            text += child.tail
    return text.strip()

def format_description(desc_node):
    if desc_node is None:
        return ""
    paragraphs = []
    for para in desc_node.findall(".//para"):
        txt = get_text_recursive(para)
        if txt:
            paragraphs.append(txt)
    return "\n\n".join(paragraphs)

def main():
    xml_dir = "docs/doxygen/xml"
    out_dir = "project/docs/classref"
    os.makedirs(out_dir, exist_ok=True)

    xml_files = glob.glob(os.path.join(xml_dir, "classgodot_1_1*.xml"))
    if not xml_files:
        print(f"No class XML files found in {xml_dir}")
        return

    print(f"Converting {len(xml_files)} Doxygen C++ class XMLs to Godot class refs...")

    for fpath in xml_files:
        try:
            tree = ET.parse(fpath)
            root = tree.getroot()
        except Exception as e:
            print(f"Failed to parse {fpath}: {e}")
            continue

        compounddef = root.find("compounddef")
        if compounddef is None:
            continue

        cpp_name = compounddef.find("compoundname").text
        class_name = cpp_name.replace("godot::", "")
        
        base_node = compounddef.find("basecompoundref")
        inherits = base_node.text if base_node is not None else ""

        brief_desc = format_description(compounddef.find("briefdescription"))
        detailed_desc = format_description(compounddef.find("detaileddescription"))

        # Build Godot XML tree
        godot_root = ET.Element("class")
        godot_root.set("name", class_name)
        if inherits:
            godot_root.set("inherits", inherits)

        brief_el = ET.SubElement(godot_root, "brief_description")
        brief_el.text = brief_desc

        desc_el = ET.SubElement(godot_root, "description")
        desc_el.text = detailed_desc

        ET.SubElement(godot_root, "tutorials")

        methods_el = ET.SubElement(godot_root, "methods")
        members_el = ET.SubElement(godot_root, "members")
        signals_el = ET.SubElement(godot_root, "signals")

        # Parse sections
        for section in compounddef.findall("sectiondef"):
            sec_kind = section.get("kind")
            for member in section.findall("memberdef"):
                kind = member.get("kind")
                prot = member.get("prot")
                if prot != "public":
                    continue

                name = member.find("name").text
                # Ignore constructor, destructor, and internal Godot methods
                if name == class_name or name == f"~{class_name}" or name.startswith("_"):
                    continue

                m_brief = format_description(member.find("briefdescription"))
                m_detail = format_description(member.find("detaileddescription"))
                m_desc = m_brief
                if m_detail:
                    m_desc = m_desc + "\n\n" + m_detail if m_desc else m_detail

                if kind == "function":
                    method_el = ET.SubElement(methods_el, "method")
                    method_el.set("name", name)

                    # Return type
                    ret_type_node = member.find("type")
                    ret_type = get_text_recursive(ret_type_node) if ret_type_node is not None else "void"
                    if not ret_type:
                        ret_type = "void"
                    ret_el = ET.SubElement(method_el, "return")
                    ret_el.set("type", ret_type)

                    # Parameters
                    for idx, param in enumerate(member.findall("param")):
                        p_type_node = param.find("type")
                        p_type = get_text_recursive(p_type_node) if p_type_node is not None else ""
                        p_name_node = param.find("declname")
                        p_name = p_name_node.text if p_name_node is not None else f"arg{idx}"

                        param_el = ET.SubElement(method_el, "param")
                        param_el.set("index", str(idx))
                        param_el.set("name", p_name)
                        param_el.set("type", p_type)

                    desc_child = ET.SubElement(method_el, "description")
                    desc_child.text = m_desc

                elif kind == "variable":
                    # Map public fields to members
                    member_el = ET.SubElement(members_el, "member")
                    member_el.set("name", name)
                    
                    var_type_node = member.find("type")
                    var_type = get_text_recursive(var_type_node) if var_type_node is not None else "Variant"
                    member_el.set("type", var_type)
                    
                    # Try to parse default value
                    init_node = member.find("initializer")
                    if init_node is not None:
                        member_el.set("default", get_text_recursive(init_node).replace("=", "").strip())

                    member_el.text = m_desc

                elif kind == "signal":
                    signal_el = ET.SubElement(signals_el, "signal")
                    signal_el.set("name", name)
                    for idx, param in enumerate(member.findall("param")):
                        p_type_node = param.find("type")
                        p_type = get_text_recursive(p_type_node) if p_type_node is not None else ""
                        p_name_node = param.find("declname")
                        p_name = p_name_node.text if p_name_node is not None else f"arg{idx}"

                        param_el = ET.SubElement(signal_el, "param")
                        param_el.set("index", str(idx))
                        param_el.set("name", p_name)
                        param_el.set("type", p_type)

                    desc_child = ET.SubElement(signal_el, "description")
                    desc_child.text = m_desc

        # Save xml
        out_fpath = os.path.join(out_dir, f"{class_name}.xml")
        
        # Format XML output nicely
        ET.indent(godot_root, space="\t", level=0)
        xml_str = ET.tostring(godot_root, encoding="utf-8", short_empty_elements=False).decode("utf-8")
        
        # Add XML declaration
        full_xml = '<?xml version="1.0" encoding="UTF-8" ?>\n' + xml_str
        
        with open(out_fpath, "w", encoding="utf-8") as f:
            f.write(full_xml)
        print(f"Generated: {out_fpath}")

if __name__ == "__main__":
    main()
