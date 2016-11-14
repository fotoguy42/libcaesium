#include <stdio.h>
#include <jpeglib.h>
#include <string.h>
#include <turbojpeg.h>

#include "jpeg.h"

//TODO Error handling

struct jpeg_decompress_struct cs_get_markers(const char *input)
{
	FILE *fp;
	struct jpeg_decompress_struct einfo;
	struct jpeg_error_mgr eerr;
	einfo.err = jpeg_std_error(&eerr);

	jpeg_create_decompress(&einfo);

	//Open the input file
	fp = fopen(input, "r");

	//Check for errors
	//TODO Use UNIX error messages
	if (fp == NULL) {
		//trigger_error(13, true, input);
	}

	//Create the IO instance for the input file
	jpeg_stdio_src(&einfo, fp);

	//Save EXIF info
	for (int m = 0; m < 16; m++) {
		jpeg_save_markers(&einfo, JPEG_APP0 + m, 0xFFFF);
	}

	jpeg_read_header(&einfo, TRUE);

	fclose(fp);

	return einfo;
}

int cs_jpeg_optimize(const char *input_file, const char *output_file, bool exif, const char *exif_src)
{
	//TODO Bug on normal compress: the input file is a bogus long string
	// Happened with a (bugged) server connection
	//File pointer for both input and output
	FILE *fp;

	//Those will hold the input/output structs
	struct jpeg_decompress_struct srcinfo;
	struct jpeg_compress_struct dstinfo;

	//Error handling
	struct jpeg_error_mgr jsrcerr, jdsterr;

	//Input/Output array coefficents
	jvirt_barray_ptr *src_coef_arrays;
	jvirt_barray_ptr *dst_coef_arrays;

	//Set errors and create the compress/decompress istances
	srcinfo.err = jpeg_std_error(&jsrcerr);
	jpeg_create_decompress(&srcinfo);
	dstinfo.err = jpeg_std_error(&jdsterr);
	jpeg_create_compress(&dstinfo);


	//Open the input file
	fp = fopen(input_file, "r");

	//Check for errors
	//TODO Use UNIX error messages
	if (fp == NULL) {
		//trigger_error(105, true, input_file);
	}

	//Create the IO istance for the input file
	jpeg_stdio_src(&srcinfo, fp);

	//Save EXIF info
	if (exif) {
		for (int m = 0; m < 16; m++) {
			jpeg_save_markers(&srcinfo, JPEG_APP0 + m, 0xFFFF);
		}
	}

	//Read the input headers
	(void) jpeg_read_header(&srcinfo, TRUE);


	//Read input coefficents
	src_coef_arrays = jpeg_read_coefficients(&srcinfo);
	//jcopy_markers_setup(&srcinfo, copyoption);

	//Copy parameters
	jpeg_copy_critical_parameters(&srcinfo, &dstinfo);

	//Set coefficents array to be the same
	dst_coef_arrays = src_coef_arrays;

	//We don't need the input file anymore
	fclose(fp);

	//Open the output one instead
	fp = fopen(output_file, "w+");
	//Check for errors
	//TODO Use UNIX error messages
	if (fp == NULL) {
		//trigger_error(106, true, output_file);
	}

	//CRITICAL - This is the optimization step
	dstinfo.optimize_coding = TRUE;
	//Progressive
	jpeg_simple_progression(&dstinfo);

	//Set the output file parameters
	jpeg_stdio_dest(&dstinfo, fp);

	//Actually write the coefficents
	jpeg_write_coefficients(&dstinfo, dst_coef_arrays);

	//Write EXIF
	if (exif) {
		if (strcmp(input_file, exif_src) == 0) {
			jcopy_markers_execute(&srcinfo, &dstinfo);
		} else {
			//For standard compression EXIF data
			struct jpeg_decompress_struct einfo = cs_get_markers(exif_src);
			jcopy_markers_execute(&einfo, &dstinfo);
			jpeg_destroy_decompress(&einfo);
		}
	}

	//Finish and free
	jpeg_finish_compress(&dstinfo);
	jpeg_destroy_compress(&dstinfo);
	(void) jpeg_finish_decompress(&srcinfo);
	jpeg_destroy_decompress(&srcinfo);

	//Close the output file
	fclose(fp);

	return 0;
}

void cs_jpeg_compress(const char *output_file, unsigned char *image_buffer, cs_jpeg_pars *options)
{
	FILE *fp;
	tjhandle tjCompressHandle;
	unsigned char *output_buffer;
	unsigned long output_size = 0;

	fp = fopen(output_file, "wb");

	//Check for errors
	//TODO Use UNIX error messages
	if (fp == NULL) {
		//trigger_error(106, true, output_file);
	}

	output_buffer = NULL;
	tjCompressHandle = tjInitCompress();

	//TODO Error checks
	tjCompress2(tjCompressHandle,
				image_buffer,
				options->width,
				0,
				options->height,
				options->color_space,
				&output_buffer,
				&output_size,
				options->subsample,
				options->quality,
				options->dct_method);

	fwrite(output_buffer, output_size, 1, fp);

	fclose(fp);
	tjDestroy(tjCompressHandle);
	tjFree(output_buffer);

}

unsigned char *cs_jpeg_decompress(const char *fileName, cs_jpeg_pars *options)
{

	//TODO I/O Error handling

	FILE *file = NULL;
	int res = 0;
	long int sourceJpegBufferSize = 0;
	unsigned char *sourceJpegBuffer = NULL;
	tjhandle tjDecompressHandle;
	int fileWidth = 0, fileHeight = 0, jpegSubsamp = 0, colorSpace = 0;

	//TODO No error checks here
	file = fopen(fileName, "rb");
	res = fseek(file, 0, SEEK_END);
	sourceJpegBufferSize = ftell(file);
	sourceJpegBuffer = tjAlloc(sourceJpegBufferSize);

	res = fseek(file, 0, SEEK_SET);
	res = fread(sourceJpegBuffer, (long) sourceJpegBufferSize, 1, file);
	tjDecompressHandle = tjInitDecompress();
	res = tjDecompressHeader3(tjDecompressHandle, sourceJpegBuffer, sourceJpegBufferSize, &fileWidth, &fileHeight,
							  &jpegSubsamp, &colorSpace);

	options->width = fileWidth;
	options->height = fileHeight;

	options->subsample = jpegSubsamp;
	options->color_space = colorSpace;

	unsigned char *temp = tjAlloc(options->width * options->height * tjPixelSize[options->color_space]);

	res = tjDecompress2(tjDecompressHandle,
						sourceJpegBuffer,
						sourceJpegBufferSize,
						temp,
						options->width,
						0,
						options->height,
						options->color_space,
						options->dct_method);

	//fwrite(temp, pars->width * pars->height * tjPixelSize[pars->color_space], 1, fopen("/Users/lymphatus/Desktop/tmp/compresse/ccc", "w"));

	tjDestroy(tjDecompressHandle);

	return temp;
}

void jcopy_markers_execute(j_decompress_ptr srcinfo, j_compress_ptr dstinfo)
{
	jpeg_saved_marker_ptr marker;

	/* In the current implementation, we don't actually need to examine the
	* option flag here; we just copy everything that got saved.
	* But to avoid confusion, we do not output JFIF and Adobe APP14 markers
	* if the encoder library already wrote one.
	*/
	for (marker = srcinfo->marker_list; marker != NULL; marker = marker->next) {
		if (dstinfo->write_JFIF_header &&
			marker->marker == JPEG_APP0 &&
			marker->data_length >= 5 &&
			GETJOCTET(marker->data[0]) == 0x4A &&
			GETJOCTET(marker->data[1]) == 0x46 &&
			GETJOCTET(marker->data[2]) == 0x49 &&
			GETJOCTET(marker->data[3]) == 0x46 &&
			GETJOCTET(marker->data[4]) == 0)
			continue;                 /* reject duplicate JFIF */
		if (dstinfo->write_Adobe_marker &&
			marker->marker == JPEG_APP0 + 14 &&
			marker->data_length >= 5 &&
			GETJOCTET(marker->data[0]) == 0x41 &&
			GETJOCTET(marker->data[1]) == 0x64 &&
			GETJOCTET(marker->data[2]) == 0x6F &&
			GETJOCTET(marker->data[3]) == 0x62 &&
			GETJOCTET(marker->data[4]) == 0x65)
			continue;                 /* reject duplicate Adobe */
		jpeg_write_marker(dstinfo, marker->marker,
						  marker->data, marker->data_length);
	}
}