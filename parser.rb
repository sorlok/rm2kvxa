#!/usr/bin/ruby1.9.1

require "fileutils"
require "chunky_png"

#Our custom lib
require File.expand_path('../native/lib/rmvx_small', __FILE__)


#Helper: read a variable-width integer; return the value and the new starting pos
def readVarIntAndPos(f, start=0)
  if f.is_a? String
    curr = f[start].ord
  else
    curr = f.read(1).ord
  end
  index = start
  res = curr&0x7F
  while curr>=0x80
    res <<= 7
    index += 1
    if f.is_a? String
      curr = f[index].ord
    else
      curr = f.read(1).ord
    end
    res |= (curr&0x7F)
  end
  return res,index+1
end


#Helper: read a varible-width integer
def readVarInt(f, start=0)
  return readVarIntAndPos(f,start)[0]
end

#GameMap
class GameMap
  attr_accessor :id, :chipsetId, :width, :height, :lowerLayer, :upperLayer

  def initialize()
    @id = 0
    @chipsetId = 1;
    @width = 20;
    @height = 15;
    @lowerLayer = [] #width x height tiles
    @upperLayer = [] #width x height tiles
  end
end

#GameDB
class GameDB
  attr_accessor :chipsets

  def initialize()
    @chipsets = {} #id=>path
  end
end


gameFolder = "./TestGame/TestGame-2000"
firstMap = "Map0030.lmu"
gameDb = "RPG_RT.ldb"
if ARGV.length >= 1
  firstMap = ARGV[0]
  gameFolder = "."
end

#Results
resMap = GameMap.new
resDb = GameDB.new


#Database
File.open("#{gameFolder}/#{gameDb}","r") {|f|
  #Read the file type.
  strLen = readVarInt(f)
  fileType = f.read(strLen)
  puts "FileType: #{fileType}"

  #Now, read "chunks"
  chunkIDs = {} #id->length (unknown)
  while true
    break if f.eof?

    #Read the chunk ID, length (skip contents)
    chunkType = readVarInt(f)
    break if chunkType==0
    chunkLength = readVarInt(f)
    data = f.read(chunkLength)

    if chunkType==0x0B
      #Actors
    elsif chunkType==0x0C
      #Skills
    elsif chunkType==0x0D
      #Items
    elsif chunkType==0x0E
      #Enemies
    elsif chunkType==0x0F
      #Troops
    elsif chunkType==0x10
      #Terrain
    elsif chunkType==0x11
      #Attributes
    elsif chunkType==0x12
      #States
    elsif chunkType==0x13
      #Animations
    elsif chunkType==0x14
      #ChipSets
      numChips,pos = readVarIntAndPos(data,0)
      puts "CHIPS: #{numChips}"

      (0...numChips).each{
        chipId,pos = readVarIntAndPos(data, pos)
        puts "  chip: #{chipId}"

        #Read chipset properties. (TODO: very copy-paste right now!)
        while true
          break if pos >= chunkLength

          #Read the chunk ID, length (skip contents)
          subChunkType,pos = readVarIntAndPos(data, pos)
          break if subChunkType==0
          subChunkLength,pos = readVarIntAndPos(data,pos)

          #TODO: Actually save it
          if subChunkType==0x02
            #Chipset file name
            chipName = data[pos...pos+subChunkLength]
            puts "  fileName: #{chipName}"

            #Try to find it:
            chipPath = "#{gameFolder}/ChipSet/#{chipName}.png"
            if File.exists? chipPath
              puts "  found: #{chipPath}"
              resDb.chipsets[chipId] = chipPath
            else
              puts "  CAN'T FIND"
            end
          end

          pos += subChunkLength
        end
        #End TODO
      }
    elsif chunkType==0x15
      #Terms
    elsif chunkType==0x16
      #System
    elsif chunkType==0x17
      #Switches
    elsif chunkType==0x18
      #Variables
    elsif chunkType==0x19
      #CommonEvents
    else
      chunkIDs[chunkType] = chunkLength
    end
  end

  #Done; print chunk types
  puts "Unknown chunks" unless chunkIDs.empty?
  chunkIDs.each{|k,v|
    puts "  #{k} => #{v}"
  } 
}


File.open("#{gameFolder}/#{firstMap}","r") {|f|
  #Read the file type.
  strLen = readVarInt(f)
  fileType = f.read(strLen)
  puts "FileType: #{fileType}"

  #Now, read "chunks"
  chunkIDs = {} #id->length (unknown)
  while true
    break if f.eof?

    #Read the chunk ID, length (skip contents)
    chunkType = readVarInt(f)
    break if chunkType==0
    chunkLength = readVarInt(f)
    data = f.read(chunkLength)
    if chunkType==0x02
      resMap.width = readVarInt(data)
    elsif chunkType==0x03
      resMap.height = readVarInt(data)
    elsif chunkType==0x0B
      #scrollType
    elsif chunkType==0x51
      #events
    elsif chunkType==0x5B
      #saveCount
    elsif chunkType==0x47
      #Lower layer
      resMap.lowerLayer = data.unpack("S<*")
      if resMap.lowerLayer.length != chunkLength/2
        raise "Error: unexpected unpacked length"
      end
    elsif chunkType==0x48
      #Upper layer
      resMap.upperLayer = data.unpack("S<*")
      if resMap.upperLayer.length != chunkLength/2
        raise "Error: unexpected unpacked length"
      end
    else
      chunkIDs[chunkType] = chunkLength
    end
  end

  #Done; print chunk types
  puts "Unknown chunks" unless chunkIDs.empty?
  chunkIDs.each{|k,v|
    puts "  #{k} => #{v}"
  } 
}

#Helper: copy a tile from old-style to new-style and flip it
def copy_tile(src, srcId, dest, destId, origin, target)
  #We need the tile's offset within its block.
  srcPt = [(srcId%3)*16 , (srcId/3)*16]
  reverse = false
  if destId.nil?
    #Special case for our weird internal tile
    destPt = [16,48]
    reverse = true
  else
    destPt = [(destId%2)*32 , (destId/2)*32]
  end

  #Now, copy it and magnify it.
  for x in (0...16)
    for y in (0...16)
      for iX in (0...2) 
        for iY in (0...2)
          srcVal = src[origin[0]+srcPt[0]+x,origin[1]+srcPt[1]+y]
          destXY = [target[0]+destPt[0]+2*x+iX,target[1]+destPt[1]+2*y+iY]
          if reverse
            if x<8
              destXY[0] += 16
            else
              destXY[0] -= 16
            end
            if y<8
              destXY[1] += 16
            else
              destXY[1] -= 16
            end
          end
          dest[destXY[0], destXY[1]] = srcVal
        end
      end
    end
  end
end


#Generic output directory
outdir = 'out'
FileUtils.rm_rf outdir
FileUtils.mkdir outdir

#Convert each chipset to a series of RMVX chipsets
resDb.chipsets.each{|chipId,chipPath|
  #Rename as "X_A1.png", etc.
  prefix = chipPath.split("/")
  prefix = prefix[prefix.length-1].split(".")
  suffix = prefix.pop
  prefix = prefix.join(".")

  #Convert the grass/forest layer (A2)
  src = ChunkyPNG::Image.from_blob(File.read(chipPath))
  dest = ChunkyPNG::Image.new(512,384, ChunkyPNG::Color::TRANSPARENT)
  (0..11).each{|blockId|
    #Get a starting corner (TopLeft)
    if blockId<=3
      origin = [
        (blockId%2)*48,
        (blockId/2)*64+128
      ]
    else
      origin = [
        (blockId%2)*48+96,
        (blockId/2)*64-128
      ]
    end

    #Give it a proper destination in the output image
    target = [
      (blockId%4)*64,
      (blockId/4)*96
    ]

    #We copy all tiles as they "should" be then overlay the central tile.
    #This is a little bit wasteful, but simple to follow.
    copy_tile(src, 0, dest, 0, origin, target)
    copy_tile(src, 2, dest, 1, origin, target)
    copy_tile(src, 3, dest, 2, origin, target)
    copy_tile(src, 5, dest, 3, origin, target)
    copy_tile(src, 9, dest, 4, origin, target)
    copy_tile(src, 11, dest, 5, origin, target)
    copy_tile(src, 7, dest, nil, origin, target)  #Special
  }
  outPath = "#{outdir}/#{prefix}_A2.#{suffix}"
  puts "Saving: #{outPath}"
  dest.save(outPath, :interlace => false)

}

#Mimic RPG Maker's module structure.
module RPG
  class BGM
    def initialize()
      @name=""
      @volume=100
      @pitch=100
    end
  end
  class BGS
    def initialize()
      @name=""
      @volume=80
      @pitch=100
    end
  end

  class Map
    attr_accessor :width, :height, :tileset_id, :data

    def initialize()
      @width=20
      @height=15
      @tileset_id=1
      @data=Table.new #TODO: <Table:0x007f226876d1d0> NOTE: This needs to be a built-in class (copy from mkxp). We can keep this local in whatever directory runs the code.

      @encounter_list=[]
      @encounter_step=30

      @events={}
      @disable_dashing=false
      @display_name=""
      @note=""

      @parallax_name=""
      @parallax_show=false
      @parallax_sx=0
      @parallax_sy=0
      @parallax_loop_x=false
      @parallax_loop_y=false
      @scroll_type=0

      @battleback1_name=""
      @battleback2_name=""
      @specify_battleback=false

      @bgm=RPG::BGM.new()
      @autoplay_bgm=false
      @bgs=RPG::BGS.new()
      @autoplay_bgs=false
    end
  end
end



#Converting the map is easy enough...
prefix = firstMap.split("/")
prefix = prefix[prefix.length-1].split(".")
suffix = prefix.pop
prefix = prefix.join(".")
prefix = prefix.sub("Map0", "Map") #Is this right?
outPath = "#{outdir}/#{prefix}.rvdata2"
puts "Saving: #{outPath}"

temp = RPG::Map.new
temp.width = resMap.width
temp.height = resMap.height
temp.tileset_id = resMap.chipsetId
temp = Marshal::dump(temp)
File.open(outPath, "wb") {|file|
  file.write temp
}



#TODO: Now, convert/Marshall the maps themselves.



























