import markdown
import os

DEFAULT_TITLE = "RNode Bootstrap Console"
SOURCES_PATH="./source"
BUILD_PATH="./build"
INPUT_ENCODING="utf-8"
OUTPUT_ENCODING="utf-8"

LXMF_ADDRESS = "8dd57a738226809646089335a6b03695"

document_start = """
<!doctype html>
<html>
<head>
<link rel="stylesheet" href="water.css?v=5">
<link rel="shortcut icon" type="image/x-icon" href="{ASSET_PATH}gfx/icon.png">
<meta charset="utf-8"/>
<title>{PAGE_TITLE}</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
</head>
<body>
<div id="load_overlay" style="background-color:#2a2a2f; position:absolute; top:0px; left:0px; width:100%; height:100%; z-index:2000;"></div>
<span class="logo">RNode Console</span>
{MENU}<hr>"""

document_end = """</body></html>"""

menu_md = """<center><span class="menu">[Start]({CONTENT_PATH}index.html) | [Replicate]({CONTENT_PATH}replicate.html) | [Guides]({CONTENT_PATH}guides.html) | [Software]({CONTENT_PATH}software/index.html)| [Help](help.html) | [Contribute]({CONTENT_PATH}contribute.html)</span></center>"""

url_maps = [
    # { "path": "", "target": "/.md"},    
]

def scan_pages(base_path):
    files = [file for file in os.listdir(base_path) if os.path.isfile(os.path.join(base_path, file)) and file[:1] != "."]
    directories = [file for file in os.listdir(base_path) if os.path.isdir(os.path.join(base_path, file)) and file[:1] != "."]

    page_sources = []

    for file in files:
        if file.endswith(".md"):
            page_sources.append(base_path+"/"+file)

    for directory in directories:
        page_sources.extend(scan_pages(base_path+"/"+directory))

    return page_sources

def get_prop(md, prop):
    try:
        pt = "["+prop+"]: <> ("
        pp = md.find(pt)
        if pp != -1:
            ps = pp+len(pt)
            pe = md.find(")", ps)
            return md[ps:pe]
        else:
            return None

    except Exception as e:
        print("Error while extracting topic property: "+str(e))
        return None

def list_topic(topic):
    base_path = SOURCES_PATH+"/"+topic
    files = [file for file in os.listdir(base_path) if os.path.isfile(os.path.join(base_path, file)) and file[:1] != "." and file != "index.md"]
    
    topic_entries = []
    for file in files:
        if file.endswith(".md"):
            fp = base_path+"/"+file
            f = open(fp, "rb")
            link_path = fp.replace(SOURCES_PATH, ".").replace(".md", ".html")

            md = f.read().decode(INPUT_ENCODING)
            topic_entries.append({
                "title": get_prop(md, "title"),
                "image": get_prop(md, "image"),
                "date": get_prop(md, "date"),
                "excerpt": get_prop(md, "excerpt"),
                "md": md,
                "file": link_path
            })

    topic_entries.sort(key=lambda e: e["date"], reverse=True)
    return topic_entries

def render_topic(topic_entries):
    md = ""
    for topic in topic_entries:
        md += "<a class=\"topic_link\" href=\""+str(topic["file"])+"\">"
        md += "<span class=\"topic\">"
        md += "<img class=\"topic_image\" src=\""+str(topic["image"])+"\"/>"
        md += "<span class=\"topic_title\">"+str(topic["title"])+"</span>"
        md += "<span class=\"topic_date\">"+str(topic["date"])+"</span>"
        md += "<span class=\"topic_excerpt\">"+str(topic["excerpt"])+"</span>"
        md += "</span>"
        md += "</a>"


    return md

def generate_html(f, root_path):
    md = f.read().decode(INPUT_ENCODING)

    page_title = get_prop(md, "title")
    if page_title == None:
        page_title = DEFAULT_TITLE
    else:
        page_title += " | "+DEFAULT_TITLE
    
    tt = "{TOPIC:"
    tp = md.find(tt)
    if tp != -1:
        ts = tp+len(tt)
        te = md.find("}", ts)
        topic = md[ts:te]
        
        rt = tt+topic+"}"
        tl = render_topic(list_topic(topic))
        print("Found topic: "+str(topic)+", rt "+str(rt))
        md = md.replace(rt, tl)

    menu_html = markdown.markdown(menu_md.replace("{CONTENT_PATH}", root_path), extensions=["markdown.extensions.fenced_code"]).replace("<p></p>", "")
    page_html = markdown.markdown(md, extensions=["markdown.extensions.fenced_code"]).replace("{ASSET_PATH}", root_path)
    page_html = page_html.replace("{LXMF_ADDRESS}", LXMF_ADDRESS)
    page_date = get_prop(md, "date")
    if page_date != None:
        page_html = page_html.replace("{DATE}", page_date)

    return document_start.replace("{ASSET_PATH}", root_path).replace("{MENU}", menu_html).replace("{PAGE_TITLE}", page_title) + page_html + document_end

source_files = scan_pages(SOURCES_PATH)

for um in url_maps:
    with open(SOURCES_PATH+"/"+um["target"], "rb") as f:
        of = BUILD_PATH+um["target"].replace(SOURCES_PATH, "").replace(".md", ".html")
        root_path = "../"
        html = generate_html(f, root_path)
        
        print("Map path   : "+str(um["path"]))
        print("Map target : "+str(um["target"]))
        print("Mapped root path: "+str(root_path))

        if not os.path.isdir(BUILD_PATH+"/"+um["path"]):
            os.makedirs(BUILD_PATH+"/"+um["path"], exist_ok=True)

        with open(BUILD_PATH+"/"+um["path"]+"/index.html", "wb") as wf:
            wf.write(html.encode(OUTPUT_ENCODING))

for mdf in source_files:
    with open(mdf, "rb") as f:
        of = BUILD_PATH+mdf.replace(SOURCES_PATH, "").replace(".md", ".html")
        root_path = "../"*(len(of.replace(BUILD_PATH+"/", "").split("/"))-1)
        html = generate_html(f, root_path)

        if not os.path.isdir(os.path.dirname(of)):
            os.makedirs(os.path.dirname(of), exist_ok=True)

        with open(of, "wb") as wf:
            wf.write(html.encode(OUTPUT_ENCODING))
