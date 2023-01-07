clean:
	@echo Cleaning...
	@-rm -rf ./build

site:
	@mkdir -p ./build
	@mkdir -p ./build/css
	@mkdir -p ./build/gfx
	@mkdir -p ./build/images
	# @mkdir -p ./build/pkgs
	python ./build.py
	@cp assets/css/* build/css/
	@cp assets/gfx/* build/gfx/
	# @cp assets/images/* build/images/
	# @cp assets/scripts/* build/scripts/
	# @cp -r ../../Reticulum/docs/manual/* build/reticulum_manual/
	# @cp -r ../../Reticulum/docs/Reticulum\ Manual.pdf build/reticulum_manual/
