[![Build Status](https://travis-ci.org/rordenlab/dcm2niix.svg?branch=master)](https://travis-ci.org/rordenlab/dcm2niix)
[![Build status](https://ci.appveyor.com/api/projects/status/7o0xp2fgbhadkgn1?svg=true)](https://ci.appveyor.com/project/neurolabusc/dcm2niix)

## About

dcm2niix is a designed to convert neuroimaging data from the DICOM format to the NIfTI format. This web page hosts the developmental source code - a compiled version for Linux, MacOS, and Windows of the most recent stable release is included with [MRIcroGL](https://www.nitrc.org/projects/mricrogl/). A full manual for this software is available in the form of a [NITRC wiki](http://www.nitrc.org/plugins/mwiki/index.php/dcm2nii:MainPage).

## License

This software is open source. The bulk of the code is covered by the BSD license. Some units are either public domain (nifti*.*, miniz.c) or use the MIT license (ujpeg.cpp). See the license.txt file for more details.

## Dependencies

This software should run on macOS, Linux and Windows typically without requiring any other software. However, if you use dcm2niix to create gz-compressed images it will be faster if you have [pigz](https://github.com/madler/pigz) installed. You can get a version of both dcm2niix and pigz compiled for your operating system by downloading [MRIcroGL](https://www.nitrc.org/projects/mricrogl/).

## Versions

23-Sept-2017
 - Swap [phase-encoding direction polarity](https://github.com/rordenlab/dcm2niix/issues/125) for Siemens images where PE is in the Column direction.
 - Sort diffusion volumes by [B-value amplitude](https://www.nitrc.org/forum/forum.php?thread_id=8396&forum_id=4703) (use "-d n"/"-d y" to turn the feature off/on).
 - BIDS tag [TotalReadoutTime](https://github.com/rordenlab/dcm2niix/issues/130) handles partial fourier, Phase Resolution, etc (Michael Harms).
 - Additional [json fields](https://github.com/rordenlab/dcm2niix/issues/127).

18-Aug-2017
 - Better BVec extraction for  [PAR/REC 4.1](https://www.nitrc.org/forum/forum.php?thread_id=8387&forum_id=4703).
 - Support for [Segami Cerescan volumes](https://www.nitrc.org/forum/forum.php?thread_id=8076&forum_id=4703).

24-July-2017
 - Compiles with recent releases of [OpenJPEG](https://github.com/neurolabusc/dcm_qa/issues/5#issuecomment-317443179) for JPEG2000 support.

23-June-2017
 - [Ensure slice timing always reported for Siemens EPI](https://github.com/neurolabusc/dcm_qa/issues/4#issuecomment-310707906)
 - [Integrates validation](https://github.com/neurolabusc/dcm_qa)
 - JSON fix (InstitutionName -> InstitutionAddress)

21-June-2017
 - Read DICOM header in 1Mb segments rather than loading whole file : reduces ram usage and [faster for systems with slow io](https://github.com/rordenlab/dcm2niix/issues/104).
 - Report [TotalReadoutTime](https://github.com/rordenlab/dcm2niix/issues/98).
 - Fix JPEG2000 support in [Superbuild](https://github.com/rordenlab/dcm2niix/issues/105).

28-May-2017
 - Remove all derived images from [Philips DTI series](http://www.nitrc.org/forum/message.php?msg_id=21025).
 - Provide some [Siemens EPI sequence details](https://github.com/rordenlab/dcm2niix/issues).

28-April-2017
 - Experimental [ECAT support](https://github.com/rordenlab/dcm2niix/issues/95).
 - Updated cmake to make JPEG2000 support easier with improved Travis and AppVeyor support [Ningfei Li](https://github.com/ningfei).
 - Supports Data/Time for images that report Data/Time (0008,002A) but not separate Date and Time (0008,0022 and 0008,0032).
 - [BIDS reports SliceTiming correctly](http://www.nitrc.org/forum/message.php?msg_id=20852).
 - Options -1..-9 to control [gz compression level](https://github.com/rordenlab/dcm2niix/issues/90).
 - Includes some [PET details in the BIDS JSON sidecar](https://github.com/rordenlab/dcm2niix/issues/87).
 - Better detection of image order for Philips 4D DICOM (reported by Jason McMorrow and Stephen Wilson).
 - [Include StudyInstanceUID and SeriesInstanceUID in filename](https://github.com/rordenlab/dcm2niix/issues/94).

7-Feb-2017
 - Can be compiled to use either Philips [Float or Display](http://www.nitrc.org/forum/message.php?msg_id=20213) intensity intercept and slope values.
 - Handle 3D Philips DICOM and [PAR/REC](https://www.nitrc.org/forum/forum.php?thread_id=7707&forum_id=4703) files where images are not stored in a spatially contiguous order.
 - Handle DICOM violations where icon is uncompressed but image data is compressed.
 - Best guess matrix for 2D slices (similar to dcm2nii, SPM and MRIconvert).
 - Linux (case sensitive filenames) now handles par/rec as well as PAR/REC.
 - Images with unknown phase encoding do not generate [BIDS entry](https://github.com/rordenlab/dcm2niix/issues/79).
 - Unified printMessage/printWarning/printError aids embedding in other projects, such as [divest](https://github.com/jonclayden/divest).

1-Nov-2016
 - AppVeyor Support (Ningfei Li & Chris Filo Gorgolewski)
 - Swap 3rd/4th dimensions for GE sequential multi-phase acquisitions (Niels Janssen).

10-Oct-2016
 - Restores/improves building for the Windows operating system using MinGW.

30-Sept-2016
 - Save ImageType (0x0008,0x0008) to BIDS.
 - Separate CT scans with different exposures.
 - Fixed issues where some compilers would generate erratic filenames for zero-padded series (e.g. "-f %3s").

21-Sept-2016
 - Reduce verbosity (reduce number of repeated warnings, less scary warnings for derived rather than raw images).
 - Re-enable custom output directory "-o" option broken by 30-Apr-2016 version.
 - Deal with mis-behaved GE CT images where slice direction across images is not consistent.
 - Add new BIDS fields (field strength, manufacturer, etc).
 - Philips PAR/REC conversion now reports inconsistent requested vs measured TR (due to prospect. motion corr.?).
 - GE: Locations In Acquisition (0054, 0081) is inaccurate if slices are interpolated, use Images In Acquisition (0020,1002) if available.
 - New filename options %d Series description (0008,103E), %z Sequence Name (0018,0024).
 - New filename options %a antenna (coil) number, %e echo number.
 - Initialize unused portions of NIfTI header to zero so multiple runs always produce identical results.
 - Supports 3D lossless JPEG saved as [multiple fragments](http://www.nitrc.org/forum/forum.php?thread_id=5872&forum_id=4703).

5-May-2016
 - Crop 3D T1 acquisitions (e.g. ./dcm2niix -x y ~/DICOM).

30-Apr-2016
 - Convert multiple files/folders with single command line invocation (e.g. ./dcm2niix -b y ~/tst ~/tst2).

22-Apr-2016
 - Detect Siemens Phase maps (phase image names end with "_ph").
 - Use current working directory if file name not specified.

12-Apr-2016
 - Provide override (command line option "-m y") to stack images of the same series even if they differ in study date/time, echo/coil number, or slice orientation. This mechanism allows users to concatenate images that break strict DICOM compliance.

22-Mar-2016
 - Experimental support for [DICOM datasets without DICOM file meta information](http://dicom.nema.org/dicom/2013/output/chtml/part10/chapter_7.html).

12-Dec-2015
 - Support PAR/REC FP values when possible (see PMC3998685).

11-Nov-2015
 - Minor refinements.

12-June-2015
 - Uses less memory (helpful for large datasets).

2-Feb-2015
 - Support for Visual Studio.
 - Remove dependency on zlib (now uses miniz).

1-Jan-2015
 - Images separated based on TE (fieldmaps).
 - Support for JPEG2000 using OpenJPEG or Jasper libraries.
 - Support for JPEG using NanoJPEG library.
 - Support for lossless JPEG using custom library.

24-Nov-2014
 - Support for CT scans with gantry tilt and varying distance between slices.

11-Oct-2014
 - Initial public release.

## Running

Command line usage is described in the [NITRC wiki](https://www.nitrc.org/plugins/mwiki/index.php/dcm2nii:MainPage#General_Usage). The minimal command line call would be `dcm2niix /path/to/dicom/folder`. However, you may want to invoke additional options, for example the call `dcm2niix -z y -f %p_%t_%s -o /path/ouput /path/to/dicom/folder` will save data as gzip compressed, with the filename based on the protocol name (%p) acquisition time (%t) and DICOM series number (%s), with all files saved to the folder "output". For more help see help: `dcm2niix -h`.

**Optional batch processing version:**

Perform a batch conversion of multiple dicoms using the configurations specified in a yaml file.
```bash
dcm2niibatch batch_config.yml
```

The configuration file should be in yaml format as shown in example `batch_config.yaml`

```yaml
Options:
  isGz:             false
  isFlipY:          false
  isVerbose:        false
  isCreateBIDS:     false
  isOnlySingleFile: false
Files:
    -
      in_dir:           /path/to/first/folder
      out_dir:          /path/to/output/folder
      filename:         dcemri
    -
      in_dir:           /path/to/second/folder
      out_dir:          /path/to/output/folder
      filename:         fa3
```

You can add as many files as you want to convert as long as this structure stays consistent. Note that a dash must separate each file.

## Build

### Build command line version with cmake (Linux, MacOS, Windows)

`cmake` and `pkg-config` (optional) can be installed as follows:

Ubuntu: `sudo apt-get install cmake pkg-config`

MacOS: `brew install cmake pkg-config`

**To build:**
```bash
mkdir build && cd build
cmake ..
make
```
`dcm2niix` will be created in the `bin` subfolder. To install on the system run `make install`.

**optional building with OpenJPEG:**

Support for JPEG2000 using OpenJPEG is optional. To build with OpenJPEG change the cmake command to `cmake -DUSE_OPENJPEG=ON ..`:

```bash
mkdir build && cd build
cmake -DUSE_OPENJPEG=ON ..
make
```

**optional batch processing version:**

The batch processing binary `dcm2niibatch` is optional. To build `dcm2niibatch` as well change the cmake command to `cmake -DBATCH_VERSION=ON ..`

This requires a compiler that supports c++11.

### Building the command line version without cmake

If you have any problems with the cmake build script described above or want to customize the software see the [COMPILE.md file for details on manual compilation](./COMPILE.md).

## Links

  - [Dcm2Bids](https://github.com/cbedetti/Dcm2Bids) uses dcm2niix to create [BIDS](http://bids.neuroimaging.io/) datasets.
  - [bidskit](https://github.com/jmtyszka/bidskit) uses dcm2niix to create [BIDS](http://bids.neuroimaging.io/) datasets.
  - [DAC2BIDS](https://github.com/dangom/dac2bids) uses dcm2niibatch to create [BIDS](http://bids.neuroimaging.io/) datasets.
  - [heudiconv](https://github.com/nipy/heudiconv) can use dcm2niix to create [BIDS](http://bids.neuroimaging.io/) datasets.
  - [nipype](https://github.com/nipy/nipype) can use dcm2niix to convert images.
  - [pydcm2niix is a Python module for working with dcm2niix](https://github.com/jstutters/pydcm2niix).
  - [dcm2niir](https://github.com/muschellij2/dcm2niir) R wrapper for dcm2niix/dcm2nii.
  - [divest](https://github.com/jonclayden/divest) R interface to dcm2niix.
  - [sci-tran dcm2niix](https://github.com/scitran-apps/dcm2niix) docker.
  - [neuro_docker](https://github.com/Neurita/neuro_docker) includes dcm2niix as part of a provides a single, static Dockerfile.
  - [neurodocker](https://github.com/kaczmarj/neurodocker) generates [custom](https://github.com/rordenlab/dcm2niix/issues/138) Dockerfiles given specific versions of neuroimaging software.
  - [dcm2niix_afni](https://afni.nimh.nih.gov/pub/dist/doc/program_help/dcm2niix_afni.html) is a version of dcm2niix included with the [AFNI](https://afni.nimh.nih.gov/) distribution.
  - [MRIcroGL](https://github.com/neurolabusc/MRIcroGL) is available for MacOS, Linux and Windows and provides a graphical interface for dcm2niix. You can get compiled copies from the [MRIcroGL NITRC web site](https://www.nitrc.org/projects/mricrogl/).