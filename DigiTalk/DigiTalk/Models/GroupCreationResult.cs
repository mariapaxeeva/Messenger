// Запрос на создание группового чата
using System.Collections.Generic;

namespace DigiTalk.Models
{
    public class GroupCreationResult
    {
        public bool Result { get; set; }
        public string Groupname { get; set; }
        public List<string> MemberUsernames { get; set; } = new List<string>();

    }
}