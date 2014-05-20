from tweepy import Stream
from tweepy import OAuthHandler
from tweepy.streaming import StreamListener
import time
import random
import os

ckey = "donoBSHOjnHt4YPG3avSmUvro"
csecret = "RARzNyiPPkg8U6DBXofQM4sbrERiz7xPFii9bJXi7RVm79IFpO"
atoken = "2509388425-pZiEKSsbY4rIlCVr0Zl6N6LU5ICRdz283ACu9is"
asecret = "7EGL5FXVkwZICn8sIjxxkTBCOSs7oEvLJTAvpCdsTvDcZ"

coords = dict()
xy = []

if os.path.exists("./tweet.csv"):
    os.remove("./tweet.csv")
db = file("./tweet.csv", "a")

class listener(StreamListener):
        def on_status(self, status):
            """ 
            print "============================"
            print "tweet text: ", status.text
            print "time: ", status.created_at
            print "time zone: ", status.user.time_zone
            print "lang: ", status.user.lang
            print "name: ", status.user.name
            print "id: ", status.user.id_str
            print "geo enabled: ", status.user.geo_enabled
            print "location: ", status.user.location
            print "place: ", status.place
            """
            try:
                coords.update(status.coordinates)
                xy = (coords.get('coordinates'))
            except:
                box = status.place.bounding_box.coordinates[0]
                xy = [(box[0][0] + box[2][0])/2, (box[0][1] + box[2][1])/2]
                pass

            global db
            db.write(status.user.id_str + "," + str(xy[1]) + "," + str(xy[0]) + "," + status.created_at.strftime("%Y-%m-%d %H:%M:%S") + "," + status.user.name.encode("ascii", "ignore") + "\r\n");

	def on_error(self, status):
		print "error ", status

def main():
    auth = OAuthHandler(ckey, csecret)
    auth.set_access_token(atoken, asecret)
    twitterStream = Stream(auth, listener())
    try:
        print "start..."
        twitterStream.filter(locations=[-125,30,-113,40])
    except KeyboardInterrupt:
        global db
        db.close()
        print "stop..."
    except Exception, e:
        print "hang..."
        time.sleep(random.randint(60,62));

if __name__ == '__main__':
    main()

