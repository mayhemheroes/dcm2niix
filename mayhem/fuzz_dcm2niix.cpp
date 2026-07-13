// In-process libFuzzer harness for dcm2niix.
//
// The old `dcm2niibatch` Mayhem target drove the CLI with `dcm2niibatch @@`, feeding the
// fuzzer a YAML *config* file (batch_config.yml) — it never fuzzed DICOM parsing at all and
// produced ~no coverage of the converter. This harness keeps the target NAME (`dcm2niibatch`)
// but points it at the SAME code path the batch converter exercises per input file: the core
// DICOM reader `readDICOMv()` in nii_dicom.cpp (step 3 of the data flow — parse a DICOM into
// TDICOMdata, decompressing pixel data as needed). dcm2niix's own API takes a filename, so we
// stage the fuzz bytes in a temp file and hand that path to the parser.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "nii_dicom.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	char tmpl[] = "/tmp/dcm2niix_fuzz_XXXXXX";
	int fd = mkstemp(tmpl);
	if (fd < 0)
		return 0;
	// Write the whole input; readDICOMv() reparses the file from its path.
	size_t off = 0;
	while (off < size) {
		ssize_t w = write(fd, data + off, size - off);
		if (w <= 0)
			break;
		off += (size_t)w;
	}
	close(fd);

	struct TDTI4D dti4D;
	memset(&dti4D, 0, sizeof(dti4D));
	// isVerbose=0 (quiet); kCompressSupport enables the built-in JPEG/JPEG-LS/JPEG2000 decoders
	// compiled into this build, so compressed transfer syntaxes reach the decoder paths.
	struct TDICOMdata d = readDICOMv(tmpl, 0, kCompressSupport, &dti4D);
	(void)d;

	unlink(tmpl);
	return 0;
}
