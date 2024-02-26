///////////////////////////////////////////////////////////////////////////////
//	AZUCO Zip file Splitter
//		r190716
//			by	AZUCO
///////////////////////////////////////////////////////////////////////////////

#include	<windows.h>
#include	<stdio.h>

#define			BUFFER_SIZE	0x10000

void	pk34_to_zip(HANDLE, LARGE_INTEGER, LARGE_INTEGER);
void	make_filename(unsigned char*, unsigned short);
void	disp_pk12_header(HANDLE, LARGE_INTEGER);
void	disp_pk34_header(HANDLE, LARGE_INTEGER);
void	disp_pk56_header(HANDLE, LARGE_INTEGER);

unsigned long	mode = 0;



int	main(int argc,char *argv[])
{
	unsigned long	fnargc=0;
	HANDLE			hsrc;
	LARGE_INTEGER	ssrc_max,					//srcのmaxサイズ保持
					psrc_rfp,					//srcの読み込みファイルポインタ
					psrc_tmp;					//srcの読み込みファイルポインタ
	unsigned char*	bpks = NULL;				//Buffer PK Search
	unsigned long	psrc_pks;					//pkサーチに使用するバッファポインタ
	unsigned long	psrc_rmn=0;					//pkサーチの際の残りバイト数

	unsigned long	tul;



///////	title	///////////////////////////////////////////////////////////////
	printf("Zip file Splitter \tProg. By AZUCO\n");



///////	help	///////////////////////////////////////////////////////////////
	if(argc==1){
		printf("azs <option> <data file>\n");
		printf("/i disp pkxx header information\n");
		printf("/g ignore extra field length,force zero set\n");
		exit(-1);
	}



///////	option	///////////////////////////////////////////////////////////////
	for(tul=1;tul<argc;tul++){
		if(argv[tul][0]=='/'){
			if (argv[tul][1] == 'i') mode = (mode & 0xffffff00) | 0x00000001;
			if (argv[tul][1] == 'g') mode = (mode & 0x00ffffff) | 0x01000000;
		}else{
			if(fnargc==0)	fnargc=tul;
		}
	}



///////	main	///////////////////////////////////////////////////////////////
	hsrc=CreateFile((LPCSTR)argv[fnargc],GENERIC_READ,0,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
	if(hsrc==(HANDLE)-1){
		printf("\tError\tsource file not found\n");
		printf("\t%ls\n",(LPCWSTR)argv[fnargc]);
		exit(-1);
	}

	GetFileSizeEx(hsrc,&ssrc_max);

	bpks=new unsigned char[BUFFER_SIZE + 4];

	switch (mode) {
	case	0x00000001:													//zipファイルのpkxxヘッダを表示する
		for (psrc_rfp.QuadPart = 0;
			psrc_rfp.QuadPart < ssrc_max.QuadPart;
			psrc_rfp.QuadPart += BUFFER_SIZE) {

			SetFilePointerEx(hsrc, psrc_rfp, NULL, FILE_BEGIN);			//サーチのため読み込み部分がラップするので、都度都度設定し直す

			if (ssrc_max.QuadPart < (psrc_rfp.QuadPart + BUFFER_SIZE)) {
				psrc_rmn = ssrc_max.LowPart - psrc_rfp.LowPart;
				ReadFile(hsrc, bpks, psrc_rmn, &tul, NULL);
				*(unsigned long *)(bpks+psrc_rmn) = 0;					//オーバーラン部分の暴発を抑止
			}else {
				psrc_rmn = BUFFER_SIZE;
				ReadFile(hsrc, bpks, psrc_rmn + 4, &tul, NULL);			//[PK??]でサーチするので、+3が必要
			}

			for (psrc_pks = 0;
				psrc_pks < psrc_rmn;
				psrc_pks++) {

				psrc_tmp.QuadPart = psrc_rfp.QuadPart + psrc_pks;

				switch (*(unsigned long*)(bpks + psrc_pks)) {
					case	0x02014b50:	disp_pk12_header(hsrc, psrc_tmp);	break;
					case	0x04034b50:	disp_pk34_header(hsrc, psrc_tmp);	break;
					case	0x06054b50:	disp_pk56_header(hsrc, psrc_tmp);	break;
				}
			}
		}
		break;

	default:															//zipファイルのpk34ヘッダを切り出して、個別のzipファイルにする
		for (	psrc_rfp.QuadPart = 0;
				psrc_rfp.QuadPart < ssrc_max.QuadPart;
				psrc_rfp.QuadPart += BUFFER_SIZE) {

			SetFilePointerEx(hsrc, psrc_rfp, NULL, FILE_BEGIN);			//サーチのため読み込み部分がラップするので、都度都度設定し直す

			if (ssrc_max.QuadPart < (psrc_rfp.QuadPart + BUFFER_SIZE)) {
				psrc_rmn = ssrc_max.LowPart - psrc_rfp.LowPart;
				ReadFile(hsrc, bpks, psrc_rmn, &tul, NULL);
				*(unsigned long*)(bpks + psrc_rmn) = 0;					//オーバーラン部分の暴発を抑止
			}else {
				psrc_rmn = BUFFER_SIZE;
				ReadFile(hsrc, bpks, psrc_rmn + 4, &tul, NULL);			//[PK34]でサーチするので、+3が必要
			}

			for (psrc_pks = 0;
				psrc_pks < psrc_rmn;
				psrc_pks++) {

				psrc_tmp.QuadPart = psrc_rfp.QuadPart + psrc_pks;

				if (*(unsigned long*)(bpks + psrc_pks) == 0x04034b50)
					pk34_to_zip(hsrc, psrc_tmp, ssrc_max);
			}
		}
		break;
	}

	if (bpks != NULL)	delete[]	bpks;
}



void	pk34_to_zip(HANDLE hsrc, LARGE_INTEGER start, LARGE_INTEGER max)
{
	unsigned char	bpk34_header[0x1e];
	unsigned char	bpk12_header[0x2e];
	unsigned char	bpk56_header[0x16];
	unsigned short	pk34_extra_field_length_true;
	HANDLE			hdst;
	unsigned char*	dst_fname	= NULL;
	unsigned char*	buf			= NULL;
	unsigned long	size;
	unsigned long	tul;
	LARGE_INTEGER	tmp;

	printf("PK34 to ZIP : Address : %016IX \n", start.QuadPart);

	SetFilePointerEx(hsrc, start, NULL, FILE_BEGIN);
	ReadFile(hsrc, bpk34_header, 0x1e, &tul, NULL);

	if(*(unsigned short*)(bpk34_header + 0x1a)==0)
	{
		printf("invalid file name length = 0\n");
		return;
	}

	dst_fname = new unsigned char[BUFFER_SIZE];
	buf = new unsigned char[BUFFER_SIZE];

	ReadFile(hsrc, dst_fname, *(unsigned short*)(bpk34_header + 0x1a), &tul, NULL);
	make_filename(dst_fname, *(unsigned short*)(bpk34_header + 0x1a));

	printf("%s\n", dst_fname);
	hdst = CreateFile((LPCSTR)dst_fname, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (hdst == (void*)-1) { printf("Can't create\n"); goto	exit; }

	pk34_extra_field_length_true = *(unsigned short*)(bpk34_header + 0x1c);
	if ((mode & 0xff000000) == 0x01000000)
		* (unsigned short*)(bpk34_header + 0x1c) = 0;

	WriteFile(hdst, &bpk34_header, 0x1e, &tul, NULL);

	tmp.QuadPart = start.QuadPart + 0x1e;
	SetFilePointerEx(hsrc, tmp, NULL, FILE_BEGIN);
	ReadFile(hsrc, buf, *(unsigned short*)(bpk34_header + 0x1a), &tul, NULL);
	WriteFile(hdst, buf, *(unsigned short*)(bpk34_header + 0x1a), &tul, NULL);

	ReadFile(hsrc, buf, pk34_extra_field_length_true, &tul, NULL);
	WriteFile(hdst, buf, *(unsigned short*)(bpk34_header + 0x1c), &tul, NULL);

	for(size= *(unsigned  long*)(bpk34_header + 0x12);;size-= BUFFER_SIZE)
	{
		if (size >= BUFFER_SIZE) {
			ReadFile(hsrc, buf, BUFFER_SIZE, &tul, NULL);
			WriteFile(hdst, buf, BUFFER_SIZE, &tul, NULL);
		}else {
			ReadFile(hsrc, buf, size, &tul, NULL);
			WriteFile(hdst, buf, size, &tul, NULL);
			break;
		}
	}



	bpk12_header[0x00] = 'P';
	bpk12_header[0x01] = 'K';
	bpk12_header[0x02] = 0x01;
	bpk12_header[0x03] = 0x02;
	*(unsigned short*)(bpk12_header + 0x04) = *(unsigned short*)(bpk34_header + 0x04);
	*(unsigned short*)(bpk12_header + 0x06) = *(unsigned short*)(bpk34_header + 0x04);
	*(unsigned short*)(bpk12_header + 0x08) = *(unsigned short*)(bpk34_header + 0x06);
	*(unsigned short*)(bpk12_header + 0x0a) = *(unsigned short*)(bpk34_header + 0x08);
	*(unsigned short*)(bpk12_header + 0x0c) = *(unsigned short*)(bpk34_header + 0x0a);
	*(unsigned short*)(bpk12_header + 0x0e) = *(unsigned short*)(bpk34_header + 0x0c);
	*(unsigned  long*)(bpk12_header + 0x10) = *(unsigned  long*)(bpk34_header + 0x0e);
	*(unsigned  long*)(bpk12_header + 0x14) = *(unsigned  long*)(bpk34_header + 0x12);
	*(unsigned  long*)(bpk12_header + 0x18) = *(unsigned  long*)(bpk34_header + 0x16);
	*(unsigned short*)(bpk12_header + 0x1c) = *(unsigned short*)(bpk34_header + 0x1a);
	*(unsigned short*)(bpk12_header + 0x1e) = *(unsigned short*)(bpk34_header + 0x1c);
	*(unsigned short*)(bpk12_header + 0x20) = 0x0000;
	*(unsigned short*)(bpk12_header + 0x22) = 0x0000;
//	*(unsigned short*)(bpk12_header + 0x24) = 0x0001;
	*(unsigned short*)(bpk12_header + 0x24) = 0x0000;
	*(unsigned  long*)(bpk12_header + 0x26) = 0x00000020;
	*(unsigned  long*)(bpk12_header + 0x2a) = 0x00000000;

	WriteFile(hdst, &bpk12_header, 0x2e, &tul, NULL);

	tmp.QuadPart = start.QuadPart + 0x1e;
	SetFilePointerEx(hsrc, tmp, NULL, FILE_BEGIN);
	ReadFile(hsrc, buf, *(unsigned short*)(bpk34_header + 0x1a), &tul, NULL);
	WriteFile(hdst, buf, *(unsigned short*)(bpk34_header + 0x1a), &tul, NULL);

	ReadFile(hsrc, buf, pk34_extra_field_length_true, &tul, NULL);
	WriteFile(hdst, buf, *(unsigned short*)(bpk34_header + 0x1c), &tul, NULL);

	ReadFile(hsrc, buf, *(unsigned short*)(bpk12_header + 0x20), &tul, NULL);
	WriteFile(hdst, buf, *(unsigned short*)(bpk12_header + 0x20), &tul, NULL);



	bpk56_header[0x00] = 'P';
	bpk56_header[0x01] = 'K';
	bpk56_header[0x02] = 0x05;
	bpk56_header[0x03] = 0x06;
	*(unsigned short*)(bpk56_header + 0x04) = 0x0000;
	*(unsigned short*)(bpk56_header + 0x06) = 0x0000;
	*(unsigned short*)(bpk56_header + 0x08) = 0x0001;
	*(unsigned short*)(bpk56_header + 0x0a) = 0x0001;
	*(unsigned  long*)(bpk56_header + 0x0c) =	0x2e + 
												*(unsigned short*)(bpk34_header + 0x1a) + 
												*(unsigned short*)(bpk34_header + 0x1c) + 
												*(unsigned short*)(bpk12_header + 0x20);
	*(unsigned  long*)(bpk56_header + 0x10) =	0x1e +
												*(unsigned short*)(bpk34_header + 0x1a) +
												*(unsigned short*)(bpk34_header + 0x1c) +
												*(unsigned  long*)(bpk34_header + 0x12);
	*(unsigned short*)(bpk56_header + 0x14) = 0x0000;

	WriteFile(hdst, &bpk56_header, 0x16, &tul, NULL);
	CloseHandle(hdst);

exit:
	if (dst_fname != NULL)	delete[]	dst_fname;
	if (buf != NULL)		delete[]	buf;
}



void	make_filename(unsigned char *buf, unsigned short max)
{
	unsigned short	counter;

	for(counter=0;counter<max;counter++)
	{
		if(buf[counter]==0x2f)
			buf[counter] =0x2d;
	}

	buf[counter] = '.';	counter++;
	buf[counter] = 'z';	counter++;
	buf[counter] = 'i';	counter++;
	buf[counter] = 'p';	counter++;
	buf[counter] = 0;	counter++;
}



void	disp_pk12_header(HANDLE hsrc, LARGE_INTEGER psrc)
{
	unsigned char	bpkh[0x2e];
	unsigned char*	bfnm;
	unsigned long	tul;

	SetFilePointerEx(hsrc, psrc, NULL, FILE_BEGIN);
	ReadFile(hsrc, bpkh, 0x2e, &tul, NULL);

	printf("Address                 %016IX \n", psrc.QuadPart);
	printf("Signature               PK12\n");
	printf("Version made            %04X\n", *(unsigned short*)(bpkh + 0x04));
	printf("Version                 %04X\n", *(unsigned short*)(bpkh + 0x06));
	printf("Generate Flug           %04X\n", *(unsigned short*)(bpkh + 0x08));
	printf("Compression Method      %04X\n", *(unsigned short*)(bpkh + 0x0a));
	printf("Dos Time                %04X\n", *(unsigned short*)(bpkh + 0x0c));
	printf("DOS Date                %04X\n", *(unsigned short*)(bpkh + 0x0e));
	printf("CRC32                   %08X\n", *(unsigned  long*)(bpkh + 0x10));
	printf("Compressed Size         %08X\n", *(unsigned  long*)(bpkh + 0x14));
	printf("Uncompressed Size       %08X\n", *(unsigned  long*)(bpkh + 0x18));
	printf("File name length        %04X\n", *(unsigned short*)(bpkh + 0x1c));
	printf("Extra Field length      %04X\n", *(unsigned short*)(bpkh + 0x1e));
	printf("File comment length     %04X\n", *(unsigned short*)(bpkh + 0x20));
	printf("Disk number start       %04X\n", *(unsigned short*)(bpkh + 0x22));
	printf("Internal File attribute %04X\n", *(unsigned short*)(bpkh + 0x24));
	printf("External File Attribute %08X\n", *(unsigned  long*)(bpkh + 0x26));
	printf("Position                %08X\n", *(unsigned  long*)(bpkh + 0x2a));

	bfnm = new unsigned char[BUFFER_SIZE];
	ReadFile(hsrc, bfnm, (DWORD) * (unsigned short*)(bpkh + 0x1c), &tul, NULL);
	bfnm[*(unsigned short*)(bpkh + 0x1c)] = 0;
	printf("File name               %s\n\n", bfnm);
	if (bfnm != NULL)		delete[]	bfnm;
}



void	disp_pk34_header(HANDLE hsrc, LARGE_INTEGER psrc)
{
	unsigned char	bpkh[0x1e];
	unsigned char* bfnm;
	unsigned long	tul;

	SetFilePointerEx(hsrc, psrc, NULL, FILE_BEGIN);
	ReadFile(hsrc, bpkh, 0x1e, &tul, NULL);

	printf("Address                 %016IX \n", psrc.QuadPart);
	printf("Signature               PK34\n");
	printf("Version                 %04X\n", *(unsigned short*)(bpkh + 0x04));
	printf("Generate Flug           %04X\n", *(unsigned short*)(bpkh + 0x06));
	printf("Compression Method      %04X\n", *(unsigned short*)(bpkh + 0x08));
	printf("Dos Time                %04X\n", *(unsigned short*)(bpkh + 0x0a));
	printf("Dos Date                %04X\n", *(unsigned short*)(bpkh + 0x0c));
	printf("CRC32                   %08X\n", *(unsigned  long*)(bpkh + 0x0e));
	printf("Compressed Size         %08X\n", *(unsigned  long*)(bpkh + 0x12));
	printf("Uncompressed Size       %08X\n", *(unsigned  long*)(bpkh + 0x16));
	printf("File name length        %04X\n", *(unsigned short*)(bpkh + 0x1a));
	printf("Extra Field length      %04X\n", *(unsigned short*)(bpkh + 0x1c));

	bfnm = new unsigned char[BUFFER_SIZE];
	ReadFile(hsrc, bfnm, (DWORD) * (unsigned short*)(bpkh + 0x1a), &tul, NULL);
	bfnm[*(unsigned short*)(bpkh + 0x1a)] = 0;
	printf("File name               %s\n\n", bfnm);
	if (bfnm != NULL)		delete[]	bfnm;
}



void	disp_pk56_header(HANDLE hsrc, LARGE_INTEGER psrc)
{
	unsigned char	bpkh[0x16];
	unsigned char* bfnm;
	unsigned long	tul;

	SetFilePointerEx(hsrc, psrc, NULL, FILE_BEGIN);
	ReadFile(hsrc, bpkh, 0x16, &tul, NULL);

	printf("Address                 %016IX \n", psrc.QuadPart);
	printf("Signature               PK56\n");
	printf("Number of Disks         %04X\n", *(unsigned short*)(bpkh + 0x04));
	printf("Disk number Start       %04X\n", *(unsigned short*)(bpkh + 0x06));
	printf("Number of Disk Entries  %04X\n", *(unsigned short*)(bpkh + 0x08));
	printf("Number of Entries       %04X\n", *(unsigned short*)(bpkh + 0x0a));
	printf("Central dir Size        %08X\n", *(unsigned  long*)(bpkh + 0x0c));
	printf("Central dir Offset      %08X\n", *(unsigned  long*)(bpkh + 0x10));
	printf("File Comment length     %04X\n", *(unsigned short*)(bpkh + 0x14));

	bfnm = new unsigned char[BUFFER_SIZE];
	ReadFile(hsrc, bfnm, (DWORD) * (unsigned short*)(bpkh + 0x14), &tul, NULL);
	bfnm[*(unsigned short*)(bpkh + 0x14)] = 0;
	printf("File comment            %s\n\n", bfnm);
	if (bfnm != NULL)		delete[]	bfnm;
}
