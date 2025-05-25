// Запрос на создание группового чата
using System.Collections.Generic;

namespace DigiTalk.Models
{
    public class GroupCreationResult
    {
        public bool Result { get; set; } // true - успех, false - ошибка
        public string? Groupname { get; set; } // Название группы
        public List<string> MemberUsernames { get; set; } = new List<string>(); // Список участников

    }
}