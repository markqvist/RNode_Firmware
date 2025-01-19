import markdown
import os
import sys
import shutil

packages = {
    "rns": "rns-0.9.1-py3-none-any.whl",
    "nomadnet": "nomadnet-0.5.7-py3-none-any.whl",
    "lxmf": "lxmf-0.6.0-py3-none-any.whl",
    "rnsh": "rnsh-0.1.5-py3-none-any.whl",
}

DEFAULT_TITLE = "RNode Bootstrap Console"
SOURCES_PATH="./source"
BUILD_PATH="./build"
PACKAGES_PATH = "../../dist_archive"
RNS_SOURCE_PATH = "../../Reticulum"
INPUT_ENCODING="utf-8"
OUTPUT_ENCODING="utf-8"

LXMF_ADDRESS = "8dd57a738226809646089335a6b03695"

document_start = """
<!DOCTYPE html>
<html>
<head>
<link rel="stylesheet" href="{ASSET_PATH}css/water.css?v=4">
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

menu_md = """<center><span class="menu">[Start]({CONTENT_PATH}index.html) | [Replicate]({CONTENT_PATH}replicate.html) | [Software]({CONTENT_PATH}software.html) | [Learn]({CONTENT_PATH}learn.html) | [Help](help.html) | [Contribute]({CONTENT_PATH}contribute.html)</span></center>"""

manual_redirect = """
<!DOCTYPE html>
<html>
<head>
<meta http-equiv="refresh" content="0; url=/m/index.html">
</head>
</html>
"""
help_redirect = """
<!DOCTYPE html>
<html>
<head>
<meta http-equiv="refresh" content="0; url=/help.html">
</head>
</html>
"""

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
        #md += "<span class=\"topic_date\">"+str(topic["date"])+"</span>"
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

    menu_html = markdown.markdown(menu_md.replace("{CONTENT_PATH}", root_path), extensions=["markdown.extensions.fenced_code", "sane_lists"]).replace("<p></p>", "")
    page_html = markdown.markdown(md, extensions=["markdown.extensions.fenced_code"]).replace("{ASSET_PATH}", root_path)
    page_html = page_html.replace("{LXMF_ADDRESS}", LXMF_ADDRESS)
    for pkg_name in packages:
        page_html = page_html.replace("{PKG_"+pkg_name+"}", "pkg/"+pkg_name+".zip")
        page_html = page_html.replace("{PKG_BASE_"+pkg_name+"}", pkg_name+".zip")
        page_html = page_html.replace("{PKG_NAME_"+pkg_name+"}", packages[pkg_name])

    page_date = get_prop(md, "date")
    if page_date != None:
        page_html = page_html.replace("{DATE}", page_date)

    return document_start.replace("{ASSET_PATH}", root_path).replace("{MENU}", menu_html).replace("{PAGE_TITLE}", page_title) + page_html + document_end

source_files = scan_pages(SOURCES_PATH)

mf = open(BUILD_PATH+"/m.html", "w")
mf.write(manual_redirect)
mf.close()
mf = open(BUILD_PATH+"/h.html", "w")
mf.write(help_redirect)
mf.close()

def optimise_manual(path):
    pm = 90
    scale_imgs = [
        ("_images/board_rnodev2.png", pm),
        ("_images/board_rnode.png", pm),
        ("_images/board_heltec32v20.png", pm),
        ("_images/board_heltec32v30.png", pm),
        ("_images/board_t3v21.png", pm),
        ("_images/board_t3v20.png", pm),
        ("_images/board_t3v10.png", pm),
        ("_images/board_t3s3.png", pm),
        ("_images/board_tbeam.png", pm),
        ("_images/board_tdeck.png", pm),
        ("_images/board_rak4631.png", pm),
        ("_images/board_tbeam_supreme.png", pm),
        ("_images/sideband_devices.webp", pm),
        ("_images/nomadnet_3.png", pm),
        ("_images/meshchat_1.webp", pm),
        ("_images/radio_is5ac.png", pm),
        ("_images/radio_rblhg5.png", pm),
        ("_static/rns_logo_512.png", 256),
        ("../images/bg_h_1.webp", pm),
    ]

    import subprocess
    import shlex
    for i,s in scale_imgs:
        fp = path+"/"+i
        resize = "convert "+fp+" -quality 25 -resize "+str(s)+" "+fp
        print(resize)
        subprocess.call(shlex.split(resize), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    remove_files = [
        "objects.inv",
        "Reticulum Manual.pdf",
        "Reticulum Manual.epub",
        "_static/styles/furo.css.map",
        "_static/scripts/furo.js.map",
        "_static/jquery-3.6.0.js",
        "_static/jquery.js",
        "static/underscore-1.13.1.js",
        "_static/_sphinx_javascript_frameworks_compat.js",
        "_static/scripts/furo.js.LICENSE.txt",
        "_static/styles/furo-extensions.css.map",
        # "_static/pygments.css",
        # "_static/language_data.js",
        # "_static/searchtools.js",
        # "searchindex.js",
    ]
    for file in remove_files:
        fp = path+"/"+file
        print("Removing file: "+str(fp))
        try:
            os.unlink(fp)
        except Exception as e:
            print("An error occurred while attempting to unlink "+str(fp)+": "+str(e))

    remove_dirs = [
        "_sources",
    ]
    for d in remove_dirs:
        fp = path+"/"+d
        print("Removing dir: "+str(fp))
        shutil.rmtree(fp)

    shutil.move(path, BUILD_PATH+"/m")

def fetch_reticulum_site():
    r_site_path = BUILD_PATH+"/r"
    if not os.path.isdir(r_site_path):
        shutil.copytree(PACKAGES_PATH+"/reticulum.network", r_site_path)
    if os.path.isdir(r_site_path+"/manual"):
        optimise_manual(r_site_path+"/manual")
    remove_files = [
        "gfx/reticulum_logo_512.png",
    ]
    for file in remove_files:
        fp = r_site_path+"/"+file
        print("Removing file: "+str(fp))
        os.unlink(fp)
    replace_paths()

def replace_paths():
    repls = [
        ("gfx/reticulum_logo_512.png", "/m/_static/rns_logo_512.png")
    ]
    for root, dirs, files in os.walk(BUILD_PATH):
        for file in files:
            fpath = root+"/"+file
            if fpath.endswith(".html"):
                print("Performing replacements in "+fpath+"")
                f = open(fpath, "rb")
                html = f.read().decode("utf-8")
                f.close()
                for s,r in repls:
                    html = html.replace(s,r)
                f = open(fpath, "wb")
                f.write(html.encode("utf-8"))
                f.close()

                # if not os.path.isdir(BUILD_PATH+"/d"):
                #     os.makedirs(BUILD_PATH+"/d")
                # shutil.move(fpath, BUILD_PATH+"/d/")


def remap_names():
    for root, dirs, files in os.walk(BUILD_PATH):
        for file in files:
            fpath = root+"/"+file
            spath = fpath.replace(BUILD_PATH, "")
            if len(spath) > 31:
                print("Path "+spath+" is too long, remapping...")
                if not os.path.isdir(BUILD_PATH+"/d"):
                    os.makedirs(BUILD_PATH+"/d")
                shutil.move(fpath, BUILD_PATH+"/d/")

            

def gz_all():
    import gzip
    for root, dirs, files in os.walk(BUILD_PATH):
        for file in files:
            fpath = root+"/"+file
            print("Gzipping "+fpath+"...")
            f = open(fpath, "rb")
            g = gzip.open(fpath+".gz", "wb")
            g.writelines(f)
            g.close()
            f.close()
            os.unlink(fpath)

from zipfile import ZipFile
for pkg_name in packages:
    pkg_file = packages[pkg_name]
    pkg_full_path = PACKAGES_PATH+"/"+pkg_file
    if os.path.isfile(pkg_full_path):
        print("Including "+pkg_file)
        z = ZipFile(BUILD_PATH+"/pkg/"+pkg_name+".zip", "w")
        z.write(pkg_full_path, pkg_full_path[len(PACKAGES_PATH+"/"):])
        z.close()
        # shutil.copy(pkg_full_path, BUILD_PATH+"/"+pkg_name)

    else:
        print("Could not find "+pkg_full_path)
        exit(1)

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

fetch_reticulum_site()
if not "--no-gz" in sys.argv:
    gz_all()

if not "--no-remap" in sys.argv:
    remap_names()
