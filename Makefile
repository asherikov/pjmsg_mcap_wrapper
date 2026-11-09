# Vendored 3rdparty code: update from upstream.
# Local fastcdr patches are kept in src/3rdparty/CMakeLists.txt.fastcdr

# releases/cpp/v2.2.0
MCAP_SHA=9b4d3e36c7f7742541df33e0cff35c95de6af2a1
MCAP_DIR=src/3rdparty/mcap

# v2.4.0
FASTCDR_SHA=d5906d720193175e898df569c8531b095648e668
FASTCDR_DIR=src/3rdparty/fastcdr


update: update_mcap update_fastcdr

addremote:
	-git remote add mcap https://github.com/foxglove/mcap.git --no-tags
	-git remote add fastcdr https://github.com/eProsima/Fast-CDR.git --no-tags

update_mcap: addremote
	git fetch --depth=1 mcap ${MCAP_SHA}
	git rm --ignore-unmatch -rf ${MCAP_DIR}
	rm -rf ${MCAP_DIR}
	env GIT_LFS_SKIP_SMUDGE=1 git read-tree --prefix=${MCAP_DIR} -u ${MCAP_SHA}
	# strip everything but the library itself: docs, tests, examples, build files
	find ${MCAP_DIR} \
		-not -wholename "${MCAP_DIR}/cpp/mcap*" \
		-not -wholename "${MCAP_DIR}/cpp" \
		-not -wholename "${MCAP_DIR}" \
		| xargs rm -rf

update_fastcdr: addremote
	git fetch --depth=1 fastcdr ${FASTCDR_SHA}
	git rm --ignore-unmatch -rf ${FASTCDR_DIR}
	rm -rf ${FASTCDR_DIR}
	env GIT_LFS_SKIP_SMUDGE=1 git read-tree --prefix=${FASTCDR_DIR} -u ${FASTCDR_SHA}
	# strip docs and tests: they are not built and there are multiple shellcheck complaints
	find ${FASTCDR_DIR} \
		-not -wholename "${FASTCDR_DIR}/cmake/common/check_configuration.cmake" \
		-not -wholename "${FASTCDR_DIR}/cmake/common" \
		-not -wholename "${FASTCDR_DIR}/cmake/packaging*" \
		-not -wholename "${FASTCDR_DIR}/cmake" \
		-not -wholename "${FASTCDR_DIR}/include*" \
		-not -wholename "${FASTCDR_DIR}/src*" \
		-not -wholename "${FASTCDR_DIR}/LICENSE" \
		-not -wholename "${FASTCDR_DIR}" \
		| xargs rm -rf
	# apply local patches
	cp ${FASTCDR_DIR}/../CMakeLists.txt.fastcdr ${FASTCDR_DIR}/CMakeLists.txt

.PHONY: update addremote update_mcap update_fastcdr
