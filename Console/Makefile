clean:
	@echo Cleaning...
	@-rm -rf ./build

dirs:
	@mkdir -p ./build
	@mkdir -p ./build/css
	@mkdir -p ./build/gfx
	@mkdir -p ./build/images

pages:
	python ./build.py

pages-debug:
	python ./build.py --no-gz
	
data:
	@echo Including assets...
	@cp assets/css/* build/css/
	@cp assets/gfx/* build/gfx/
	# @cp assets/images/* build/images/
	# @cp assets/scripts/* build/scripts/
	# @cp -r ../../Reticulum/docs/manual/* build/reticulum_manual/
	# @cp -r ../../Reticulum/docs/Reticulum\ Manual.pdf build/reticulum_manual/

site: dirs data pages

local: dirs data pages-debug

serve:
	python -m http.server 7777 --bind 127.0.0.1 --directory ./build
