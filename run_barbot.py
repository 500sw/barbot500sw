import serial, pygame, sys, time, random, os, argparse, csv
from pygame import locals


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("device", help="what serial device to use")
    parser.add_argument("-l", "--length", type=int, default=20, help="song length")
    parser.add_argument("-b", "--baud", default=38400, help="baud rate")
    parser.add_argument("--dances", default="./dance_sounds/")
    parser.add_argument("--starts", default="./start_sounds/")
    args = parser.parse_args()
    print args

    cocktail = {
        'awesome'   : 5,
        'sexy'      : 4,
        'violent'   : 3,
        'smooth'    : 2,
        'shy'       : 1,
        'terrible'  : 0,
        'fail'      : 100}

    serDevice = serial.Serial(args.device, args.baud)
    pygame.init()
    totaljoy = pygame.joystick.get_count()
    if totaljoy < 1:
        print "Didn't find a DDR mat"
        sys.exit()
    ddrmat = pygame.joystick.Joystick(totaljoy-1)


    try:
        while 1:
            line = str(serDevice.readline()).strip()
            if line == "countdown":
                print "woo counting"
                playSoundFromDir(args.starts)
            
            elif line == "start":
                song = playSoundFromDir(args.dances)
                dance = captureDance(ddrmat, args.length, serDevice)
                pygame.mixer.music.stop()
                steps, combos, clusters = scoreDance(dance)
                
                print "scored dance: steps={0}, combos={1}, clusters={2}".format(steps, combos, clusters)
                drink = chooseDrink(args.length, steps, combos, clusters)
                print "dance is '{0}' -> cocktail #{1}".format(drink, cocktail[drink])
                log([time.time(), song, steps, combos, clusters, drink, dance])
                time.sleep(1)
                serDevice.write("drink {0} \n\n".format( int(cocktail[drink]) ))
            else:
                print "Arduino: {0}".format(line)
    except KeyboardInterrupt:
        print " .. Bye! :D"
    except serial.SerialException:
        print " .. Serial disconnected, cya! :D"


def playSoundFromDir(dir):
    files = os.listdir(dir)
    index = random.randrange(0, len(files))
    print "playing '{0}' from dir '{1}'".format(files[index], dir)
    try:
        pygame.mixer.music.load(dir + files[index])
        pygame.mixer.music.play()
        return files[index]
    except pygame.error:
        return playSoundFromDir(dir)
    
    
def chooseDrink(time_dancing, steps, combos, clusters):
    if (steps * 60 / time_dancing) > 80: #fast threshold speed
        if clusters < 2:   #good rhythm
            return "awesome"
        elif clusters < 3: #ok rhythm
            return "sexy"
        else: #bad rhythm
            return "violent"
    else: # slow
        if clusters < 2:  #good rhythm
            return "smooth"
        elif clusters < 3: #ok rhythm
            return "shy"
        else: #bad rhythm
            return "terrible"
    

def scoreDance(dance):
    nbSteps = len(dance)
    nbCombos = 0
    timeDifferentials = []
    for i in range(len(dance)):
        if (i > 0):
            dt = dance[i][0] - dance[i-1][0]
            nbCombos += 1 if (dt < 100) else 0
            timeDifferentials.append(dt)

    nbOfClusters = 1
    timeDifferentials.sort() #sort the array from lower to higher
    for i in range (0, len(timeDifferentials)):
        if timeDifferentials[i] - timeDifferentials[i-1] > 100:
            nbOfClusters += 1
    return nbSteps, nbCombos, nbOfClusters


def captureDance(ddrmat, seconds, serialDevice=None):
    ddrmat.init()
    startTime = time.time()
    rhythmArray = []
    while (time.time() - startTime < seconds):
        event = pygame.event.poll()
        if event.type == pygame.locals.JOYBUTTONDOWN:
            rhythmArray.append( [time.time()*1000, event.button] )
            if (serialDevice != None):
                serialDevice.write("foot "+str(event.button))
    return rhythmArray

def log(data):
    logfile  = open('log.csv', "a")
    log = csv.writer(logfile, delimiter='\t', quotechar='"', quoting=csv.QUOTE_ALL)
    log.writerow(data)
    logfile.close()

if __name__ == '__main__':
    main()

