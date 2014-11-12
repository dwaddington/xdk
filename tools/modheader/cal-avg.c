/*
   eXokernel Development Kit (XDK)

   Based on code by Samsung Research America Copyright (C) 2013
 
   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.

   As a special exception, if you link the code in this file with
   files compiled with a GNU compiler to produce an executable,
   that does not cause the resulting executable to be covered by
   the GNU Lesser General Public License.  This exception does not
   however invalidate any other reasons why the executable file
   might be covered by the GNU Lesser General Public License.
   This exception applies to code released by its copyright holders
   in files containing the exception.  
*/




#include <stdio.h>

int main(int argc, char **argv)
{
	FILE  *fp;
	char line[1024];
	float timeVal;
	float max = 0;
	float min = 0;
	float total = 0;
	float num = 0;

	fp = fopen(argv[1], "r");	
	if (!fp)  {
		printf("Cannot open file %s\n", argv[1]);
		return 0;
	}
	while (NULL != fgets(line, 1024, fp)) {
		num++;
		sscanf(line, "%f\n", &timeVal);
	//	printf("Time is %.6f\n", timeVal);
		if (max==0 || max <timeVal) max=timeVal;
		if (min==0 || min >timeVal) min=timeVal;
		total += timeVal;
	}
	fclose(fp);
	printf("Max: %.6f  Min: %.6f  Avg: %.6f\n", max, min, total/(float)num);
}
