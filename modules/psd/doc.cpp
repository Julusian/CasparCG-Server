/*
* Copyright (c) 2011 Sveriges Television AB <info@casparcg.com>
*
* This file is part of CasparCG (www.casparcg.com).
*
* CasparCG is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* CasparCG is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with CasparCG. If not, see <http://www.gnu.org/licenses/>.
*
* Author: Niklas P Andersson, niklas.p.andersson@svt.se
*/

#include "doc.h"
#include <iostream>

namespace caspar { namespace psd {

Document::Document() : channels_(0), width_(0), height_(0), depth_(0), color_mode_(InvalidColorMode)
{
}


bool Document::parse(const std::wstring& filename)
{
	bool result = true;
	
	try
	{
		filename_ = filename;
		input_.Open(filename_);
		read_header();
		read_color_mode();
		read_image_resources();
		read_layers();
	}
	catch(std::exception& ex)
	{
		std::clog << "ooops!" << std::endl;
		result = false;
	}

	return result;
}

void Document::read_header()
{
	unsigned long signature = input_.read_long();
	unsigned short version = input_.read_short();
	if(!(signature == '8BPS' && version == 1))
		throw PSDFileFormatException();

	input_.discard_bytes(6);
	channels_= input_.read_short();
	height_ = input_.read_long();
	width_ = input_.read_long();
	depth_ = input_.read_short();	//bits / channel

	color_mode_ = int_to_color_mode(input_.read_short());
}

void Document::read_color_mode()
{
	unsigned long length = input_.read_long();
	input_.discard_bytes(length);
}

void Document::read_image_resources()
{
	unsigned long section_length = input_.read_long();

	if(section_length > 0)
	{
		unsigned long end_of_section = input_.current_position() + section_length;
	
		try
		{
			while(input_.current_position() < end_of_section)
			{
				unsigned long signature = input_.read_long();
				if(signature != '8BIM')
					throw PSDFileFormatException();

				unsigned short resource_id = input_.read_short();
				
				std::wstring name = input_.read_pascal_string(2);

				unsigned long resource_length = input_.read_long();

				//TODO: read actual data
				switch(resource_id)
				{
				case 1005:
					{	
						////resolutionInfo
						//struct ResolutionInfo
						//{
						//Fixed hRes;
						//int16 hResUnit;
						//int16 widthUnit;
						//Fixed vRes;
						//int16 vResUnit;
						//int16 heightUnit;
						//};
					}
					break;

				case 1006:
					{
						//names of alpha channels
					}
					break;
				
				case 1008:
					{} break;	//caption

				case 1010:
					{} break;	//background color

				case 1024:
					{} break;	//layer state info (2 bytes containing the index of target layer (0 = bottom layer))

				case 1026:
					{} break;	//layer group information

				case 1028:
					{} break;	//IPTC-NAA. File Info...
				case 1029:	
					{} break;	//image for raw format files
				case 1036:
					break;		//thumbnail resource
				case 1045:		//unicode Alpha names (Unicode string (4 bytes length followed by string))
					break;
				case 1053:
					break;		//alpha identifiers (4 bytes of length, followed by 4 bytes each for every alpha identifier.)
				case 1060:
					break;		//XMP metadata
				case 1065:
					break;		//layer comps
				case 1069:
					break;		//layer selection ID(s)
				case 1072:		//layer group(s) enabled id
					break;
				case 1075:		//timeline information
					break;
				case 1077:		//DisplayInfo
					break;
				case 2999:		//name of clipping path
					break;

				default:
					{
						if(resource_id >= 2000 && resource_id <=2997)	//path information
						{
						}
						else if(resource_id >= 4000 && resource_id <= 4999)	//plug-in resources
						{
						}
					}
					break;
				}

				input_.discard_bytes(resource_length);
			}
		}
		catch(PSDFileFormatException& ex)
		{
			//if an error occurs, just skip this section
			input_.set_position(end_of_section);
		}
	}
}


void Document::read_layers()
{
	//"Layer And Mask information"
	unsigned long total_length = input_.read_long();	//length of "Layer and Mask information"
	unsigned long end_of_layers = input_.current_position() + total_length;

	try
	{
		//"Layer info section"
		{
			unsigned long layer_info_length = input_.read_long();	//length of "Layer info" section
			unsigned long end_of_layers_info = input_.current_position() + layer_info_length;

			short layers_count = abs(static_cast<short>(input_.read_short()));
			std::clog << "Expecting " << layers_count << " layers" << std::endl;

			for(unsigned short layerIndex = 0; layerIndex < layers_count; ++layerIndex)
			{
				layers_.push_back(Layer::create(input_));	//each layer reads it's "layer record"
				std::clog << "Added layer: " << std::string(layers_[layerIndex]->name().begin(), layers_[layerIndex]->name().end()) << std::endl;
			}

			unsigned long channel_data_start = input_.current_position();	//TODO: remove. For debug purposes only

			auto end = layers_.end();
			for(auto layer_it = layers_.begin(); layer_it != end; ++layer_it)
			{
				(*layer_it)->read_channel_data(input_);	//each layer reads it's "image data"
			}

			input_.set_position(end_of_layers_info);
		}

		unsigned long cp = input_.current_position();
		//global layer mask info
		unsigned long global_layer_mask_length = input_.read_long();
		input_.discard_bytes(global_layer_mask_length);

	}
	catch(std::exception& ex)
	{
		std::clog << "big oops";
		input_.set_position(end_of_layers);
	}
}

}	//namespace psd
}	//namespace caspar
